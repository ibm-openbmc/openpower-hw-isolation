#include "watch.hpp"

#include "common_types.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>

namespace hw_isolation
{
namespace watch
{
namespace inotify
{

using namespace std::string_literals;
using namespace phosphor::logging;

Watch::Watch(const sd_event* eventObj, const int inotifyFlagsToWatch,
             const uint32_t eventMasksToWatch, const uint32_t eventsToWatch,
             const std::filesystem::path& fileToWatch,
             WatcherHandler watcherHandler) :
    _inotifyFlagsToWatch(inotifyFlagsToWatch),
    _eventMasksToWatch(eventMasksToWatch), _eventsToWatch(eventsToWatch),
    _fileToWatch(fileToWatch), _watcherHandler(watcherHandler),
    _watchDescriptor(-1), _watchFileDescriptor(inotifyInit())
{
    if (!std::filesystem::exists(_fileToWatch))
    {
        log<level::ERR>(fmt::format("Given path [{}] doesn't exist to watch",
                                    _fileToWatch.string())
                            .c_str());
        throw type::CommonError::InvalidArgument();
    }

    _watchDescriptor = inotify_add_watch(
        _watchFileDescriptor(), _fileToWatch.c_str(), eventMasksToWatch);
    if (_watchDescriptor == -1)
    {
        log<level::ERR>(
            fmt::format("inotify_add_watch call failed with ErrNo[{}] "
                        "ErrMsg[{}]",
                        errno, strerror(errno))
                .c_str());
        throw type::CommonError::InternalFailure();
    }

    auto rc = sd_event_add_io(const_cast<sd_event*>(eventObj), nullptr,
                              _watchFileDescriptor(), _eventsToWatch,
                              sdEventHandler, this);
    if (rc < 0)
    {
        // Failed to add to event loop
        log<level::ERR>(
            fmt::format("sd_event_add_io call failed with ErrNo[{}] "
                        "ErrMsg[{}]",
                        errno, strerror(errno))
                .c_str());
        throw type::CommonError::InternalFailure();
    }
}

Watch::~Watch()
{
    if ((_watchFileDescriptor() >= 0) && (_watchDescriptor >= 0))
    {
        inotify_rm_watch(_watchFileDescriptor(), _watchDescriptor);
    }
}

int Watch::inotifyInit()
{
    auto fd = inotify_init1(_inotifyFlagsToWatch);

    if (-1 == fd)
    {
        log<level::ERR>(fmt::format("inotify_init1 call failed with ErrNo[{}] "
                                    "ErrMsg[{}]",
                                    errno, strerror(errno))
                            .c_str());
        throw type::CommonError::InternalFailure();
    }

    return fd;
}

int Watch::sdEventHandler(sd_event_source*, int fd, uint32_t eventsOfFd,
                          void* userdata)
{
    auto watchPtr = static_cast<Watch*>(userdata);

    if (!(eventsOfFd & watchPtr->_eventsToWatch))
    {
        return 0;
    }

    // Maximum inotify events supported in the buffer
    constexpr auto maxBytes = sizeof(struct inotify_event) + NAME_MAX + 1;
    uint8_t buffer[maxBytes];

    auto bytes = read(fd, buffer, maxBytes);
    if (0 > bytes)
    {
        // Failed to read inotify event
        // Report error and return
        log<level::ERR>(fmt::format("read call failed with ErrNo[{}] "
                                    "ErrMsg[{}]",
                                    errno, strerror(errno))
                            .c_str());
        report<type::CommonError::InternalFailure>();
        return 0;
    }

    auto offset = 0;

    while (offset < bytes)
    {
        auto receivedEvent = reinterpret_cast<inotify_event*>(&buffer[offset]);
        auto callWatcherHandler =
            receivedEvent->mask & watchPtr->_eventMasksToWatch;

        if (callWatcherHandler)
        {
            watchPtr->_watcherHandler();
        }

        offset += offsetof(inotify_event, name) + receivedEvent->len;
    }

    return 0;
}

} // namespace inotify
} // namespace watch
} // namespace hw_isolation
