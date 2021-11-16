// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common/isolatable_hardwares.hpp"
#include "hw_isolation_event/event.hpp"
#include "hw_isolation_record/entry.hpp"
#include "hw_isolation_record/manager.hpp"

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
     *  @param[in] hwIsolationRecordMgr - the hardware isolation record manager
     */
    Manager(sdbusplus::bus::bus& bus, record::Manager& hwIsolationRecordMgr);

    /**
     * @brief Used to create hardware status event for all hardware.
     *
     * @return NULL
     *
     * @note This function will skip to create
     *       the hardware status event if any failures while
     *       processing all hardware.
     */
    void restoreHardwaresStatusEvent();

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
     * @brief Used to get isolatable hardware details
     */
    isolatable_hws::IsolatableHWs _isolatableHWs;

    /**
     * @brief Used to get the hardware isolation record details
     */
    record::Manager& _hwIsolationRecordMgr;

    /**
     * @brief List of pdbg target (aka hardware) class to create
     *        the hardware status event.
     */
    std::vector<std::string> _requiredHwsPdbgClass;

    /**
     * @brief The list of D-Bus match objects to process
     *        the interested D-Bus signal if catched.
     */
    std::vector<std::unique_ptr<sdbusplus::bus::match::match>>
        _dbusSignalWatcher;

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

    /**
     * @brief Used to take appropriate action when the HostState changed
     *
     * @param[in] message - The D-Bus signal message
     *
     * @return NULL
     */
    void onHostStateChange(sdbusplus::message::message& message);

    /**
     * @brief Used to take appropriate action when the BootProgress changed
     *
     * @param[in] message - The D-Bus signal message
     *
     * @return NULL
     */
    void onBootProgressChange(sdbusplus::message::message& message);
};

} // namespace hw_status
} // namespace event
} // namespace hw_isolation
