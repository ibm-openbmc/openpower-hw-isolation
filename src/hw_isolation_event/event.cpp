// SPDX-License-Identifier: Apache-2.0

#include "hw_isolation_event/event.hpp"

#include <fmt/format.h>

#include <cereal/archives/binary.hpp>
#include <phosphor-logging/elog-errors.hpp>

#include <ctime>
#include <filesystem>
#include <fstream>

// Associate Event Class with version number
constexpr uint32_t Cereal_EventClassVersion = 1;
CEREAL_CLASS_VERSION(hw_isolation::event::Event, Cereal_EventClassVersion);

namespace hw_isolation
{
namespace event
{
namespace fs = std::filesystem;

using namespace phosphor::logging;

Event::Event(sdbusplus::bus::bus& bus, const std::string& objPath,
             const EventId eventId, const EventSeverity eventSeverity,
             const EventMsg& eventMsg,
             const type::AssociationDef& associationDef,
             const bool reqDeserialize) :
    type::ServerObject<EventInterface, AssociationDefInterface>(
        bus, objPath.c_str(),
        type::ServerObject<EventInterface,
                           AssociationDefInterface>::action::defer_emit),
    _bus(bus), _eventId(eventId)
{
    // Setting properties which are defined in EventInterface
    message(eventMsg);
    severity(eventSeverity);

    // Set creation time of the event
    std::time_t timeStamp = std::time(nullptr);
    timestamp(timeStamp);

    // Add the associations which contain other required objects path
    // that helps event object consumer to pull other additional information
    // of this event
    associations(associationDef);

    if (reqDeserialize)
    {
        deserialize();
    }
    else
    {
        // Emit the signal for the event object creation since it deferred
        // in interface constructor
        this->emit_object_added();

        serialize();
    }
}

Event::~Event()
{
    fs::path path{fmt::format(HW_ISOLATION_EVENT_PERSIST_PATH, _eventId)};
    if (fs::exists(path))
    {
        fs::remove(path);
    }
}

void Event::serialize()
{
    fs::path path{fmt::format(HW_ISOLATION_EVENT_PERSIST_PATH, _eventId)};
    try
    {
        std::ofstream os(path.c_str(), std::ios::binary);
        cereal::BinaryOutputArchive oarchive(os);
        oarchive(*this);
    }
    catch (const cereal::Exception& e)
    {
        log<level::ERR>(fmt::format("Exception: [{}] during serialize the "
                                    "hardware isolation status event into {}",
                                    e.what(), path.string())
                            .c_str());
        fs::remove(path);
    }
}

void Event::deserialize()
{
    fs::path path{fmt::format(HW_ISOLATION_EVENT_PERSIST_PATH, _eventId)};
    try
    {
        if (fs::exists(path))
        {
            std::ifstream is(path.c_str(), std::ios::in | std::ios::binary);
            cereal::BinaryInputArchive iarchive(is);
            iarchive(*this);
        }
    }
    catch (const cereal::Exception& e)
    {
        log<level::ERR>(fmt::format("Exception: [{}] during deserialize the "
                                    "hardware isolation status event from {}",
                                    e.what(), path.string())
                            .c_str());
        fs::remove(path);
    }
}

} // namespace event
} // namespace hw_isolation
