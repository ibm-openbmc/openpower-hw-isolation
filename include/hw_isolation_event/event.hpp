// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common/common_types.hpp"

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Logging/Event/server.hpp>

namespace hw_isolation
{
namespace event
{

using EventInterface = sdbusplus::xyz::openbmc_project::Logging::server::Event;
using EventId = uint32_t;
using EventSeverity =
    sdbusplus::xyz::openbmc_project::Logging::server::Event::SeverityLevel;
using EventMsg = std::string;

using AssociationDefInterface =
    sdbusplus::xyz::openbmc_project::Association::server::Definitions;

/**
 * @class Event
 *
 * @brief Hardware isolation event implementation
 *
 * @details Implemented the below interfaces
 *          xyz.openbmc_project.Logging.Event
 *          xyz.openbmc_project.Association.Definitions
 *
 */
class Event : public type::ServerObject<EventInterface, AssociationDefInterface>
{
  public:
    Event() = delete;
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    Event(Event&&) = delete;
    Event& operator=(Event&&) = delete;
    virtual ~Event() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *
     *  @param[in] bus - bus to attach with dbus event object path.
     *  @param[in] objPath - event dbus object path to attach.
     *  @param[in] eventId - the dbus event id.
     *  @param[in] eventSeverity - the severity of the event.
     *  @param[in] eventMsg - the message of the event
     *  @param[in] associationDef - the association to hold other dbus
     *                              object path along with event object.
     */
    Event(sdbusplus::bus::bus& bus, const std::string& objPath,
          const EventId eventId, const EventSeverity eventSeverity,
          const EventMsg& eventMsg, const type::AssociationDef& associationDef);

  private:
    /** @brief Attached bus connection */
    sdbusplus::bus::bus& _bus;

    /** @brief The id of isolated hardware dbus event */
    EventId _eventId;

}; // end of Event class

} // namespace event
} // namespace hw_isolation
