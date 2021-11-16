// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hw_isolation_event/event.hpp"

#include <sdbusplus/bus.hpp>

namespace hw_isolation
{
namespace event
{
namespace hw_status
{

using HwStatusEvents = std::map<EventId, std::unique_ptr<Event>>;

/**
 *  @class Manager
 *
 *  @brief Hardware status event manager implementation.
 *
 */
class Manager
{
  public:
    Manager() = delete;
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(Manager&&) = delete;
    virtual ~Manager() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *
     *  @param[in] bus - Bus to attach to.
     */
    Manager(sdbusplus::bus::bus& bus);

  private:
    /**
     * @brief Attached bus connection
     */
    sdbusplus::bus::bus& _bus;

    /**
     * @brief Last created event id
     */
    EventId _lastEventId;

    /**
     * @brief Hardware status event list
     */
    HwStatusEvents _hwStatusEvents;

    /**
     * @brief Create the hardware status event dbus object
     *
     * @param[in] eventSeverity - the severity of the event.
     * @param[in] eventMsg - the message of the event.
     * @param[in] hwInventoryPath - the hardware inventory path.
     * @param[in] bmcErrorLogPath - the bmc error log object path.
     *
     * @return the created hardware status event dbus object path on success
     *         Empty optional on failures.
     */
    std::optional<sdbusplus::message::object_path> createEvent(
        const EventSeverity& eventSeverity, const EventMsg& eventMsg,
        const std::string& hwInventoryPath, const std::string& bmcErrorLogPath);
};

} // namespace hw_status
} // namespace event
} // namespace hw_isolation
