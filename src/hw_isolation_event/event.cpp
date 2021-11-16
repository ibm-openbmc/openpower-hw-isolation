// SPDX-License-Identifier: Apache-2.0

#include "hw_isolation_event/event.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>

#include <ctime>

namespace hw_isolation
{
namespace event
{

using namespace phosphor::logging;

Event::Event(sdbusplus::bus::bus& bus, const std::string& objPath,
             const EventId eventId, const EventSeverity eventSeverity,
             const EventMsg& eventMsg,
             const type::AssociationDef& associationDef) :
    type::ServerObject<EventInterface, AssociationDefInterface>(
        bus, objPath.c_str(), true),
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

    // Emit the signal for the event object creation since it deferred in
    // interface constructor
    this->emit_object_added();
}

} // namespace event
} // namespace hw_isolation
