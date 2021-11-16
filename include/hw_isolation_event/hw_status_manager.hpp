// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hw_isolation_event/event.hpp"
#include "hw_isolation_record/entry.hpp"

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

    /**
     * @brief Used to clear the old hardwares status event
     *
     * @return NULL
     */
    void clearHardwaresStatusEvent();

    /**
     * @brief Used to get the isolated hardware record status
     *
     * @param[in] recSeverity - the severity of the isolated hardware record
     *
     * @return the pair<EventMsg, EventSeverity> of the isolated hardware
     *         record on success
     *         the pair<"Unknown", Warning> on failure
     */
    std::pair<event::EventMsg, event::EventSeverity> getIsolatedHwStatusInfo(
        const record::entry::EntrySeverity& recSeverity);
};

} // namespace hw_status
} // namespace event
} // namespace hw_isolation
