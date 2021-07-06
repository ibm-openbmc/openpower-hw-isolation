#pragma once

#include <sys/inotify.h>
#include <systemd/sd-event.h>
#include <unistd.h>

#include <filesystem>
#include <functional>
#include <map>

namespace hw_isolation
{
namespace watch
{
namespace inotify
{

using WatcherHandler = std::function<void(void)>;

/** @class FD
 *
 *  @brief RAII wrapper for file descriptor.
 */
class FD
{
  private:
    /** @brief File descriptor */
    int fd = -1;

  public:
    FD() = delete;
    FD(const FD&) = delete;
    FD& operator=(const FD&) = delete;
    FD(FD&&) = delete;
    FD& operator=(FD&&) = delete;

    /** @brief Saves File descriptor and uses it to do file operation
     *
     *  @param[in] fd - File descriptor
     */
    FD(int fd) : fd(fd)
    {}

    ~FD()
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }

    int operator()() const
    {
        return fd;
    }
};

/** @class Watch
 *
 *  @brief Adds inotify watch on directory.
 *
 */
class Watch
{
  public:
    Watch(const Watch&) = delete;
    Watch& operator=(const Watch&) = delete;
    Watch(Watch&&) = default;
    Watch& operator=(Watch&&) = default;

    /** @brief Create watcher for interested file to take some action if
     *         interested events occured on that file.
     *
     *  @param[in] eventObj event loop object to attach fd of watching file.
     *  @param[in] inotifyFlagsToWatch inotify flags to watch
     *  @param[in] eventMasksToWatch mask of events to watch
     *  @param[in] eventsToWatch events to watch
     *  @param[in] fileToWatch file path to be watch
     *  @param[in] watcherHandler to take further action if the interested
     *             events are occured
     */
    Watch(const sd_event* eventObj, const int inotifyFlagsToWatch,
          const uint32_t eventMasksToWatch, const uint32_t eventsToWatch,
          const std::filesystem::path& fileToWatch,
          WatcherHandler watcherHandler);

    /* @brief Remove inotify watch and close fd's */
    ~Watch();

  private:
    /** @brief inotify flags */
    int _inotifyFlagsToWatch;

    /** @brief Mask of events */
    uint32_t _eventMasksToWatch;

    /** @brief Events to be watched */
    uint32_t _eventsToWatch;

    /** @brief File path to be watched */
    std::filesystem::path _fileToWatch;

    /** Watcher callback */
    WatcherHandler _watcherHandler;

    /** @brief dump file directory watch descriptor */
    int _watchDescriptor;

    /** @brief file descriptor manager */
    FD _watchFileDescriptor;

    /**  initialize an inotify instance and returns file descriptor */
    int inotifyInit();

    /** @brief sd-event callback.
     *  @details Collects the files and event info and call the
     *           watcher handler for further action.
     *
     *  @param[in] sdEventSrc event source, floating (unused) in our case
     *  @param[in] fd inotify fd
     *  @param[in] eventsOfFd events that matched for fd
     *  @param[in] userdata pointer to Watch object
     *
     *  @returns 0 on success
     *           -1 on failure
     */
    static int sdEventHandler(sd_event_source* sdEventSrc, int fd,
                              uint32_t eventsOfFd, void* userdata);
};

} // namespace inotify
} // namespace watch
} // namespace hw_isolation
