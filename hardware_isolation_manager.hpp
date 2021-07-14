// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common_types.hpp"
#include "hardware_isolation_entry.hpp"
#include "isolatable_hardwares.hpp"
#include "openpower_guard_interface.hpp"
#include "org/open_power/HardwareIsolation/Create/server.hpp"
#include "watch.hpp"
#include "xyz/openbmc_project/Collection/DeleteAll/server.hpp"
#include "xyz/openbmc_project/HardwareIsolation/Create/server.hpp"

namespace hw_isolation
{

using CreateInterface =
    sdbusplus::xyz::openbmc_project::HardwareIsolation::server::Create;
using OP_CreateInterface =
    sdbusplus::org::open_power::HardwareIsolation::server::Create;

using IsolatedHardwares =
    std::map<entry::EntryId, std::unique_ptr<entry::Entry>>;

using DeleteAllInterface =
    sdbusplus::xyz::openbmc_project::Collection::server::DeleteAll;

/**
 *  @class Manager
 *
 *  @brief Hardware isolation manager implementation
 *
 *  @details Implemetation for below interfaces
 *           xyz.openbmc_project.HardwareIsolation.Create
 *           xyz.openbmc_project.Collection.DeleteAll
 *           org.open_power.HardwareIsolation.Create
 */
class Manager :
    public type::ServerObject<CreateInterface, OP_CreateInterface,
                              DeleteAllInterface>
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
     *  @param[in] path - Path to attach at.
     *  @param[in] eventLoop - Attached event loop on bus.
     */
    Manager(sdbusplus::bus::bus& bus, const std::string& objPath,
            const sd_event* eventLoop);

    /**
     *  @brief Implementation for Create
     *
     *  @param[in] isolateHardware - The hardware inventory path which is
     *                               needs to isolate.
     *  @param[in] severity - The severity of isolating hardware.
     *
     *  @return path The path of created
     * xyz.openbmc_project.HardwareIsolation.Entry object.
     */
    sdbusplus::message::object_path create(
        sdbusplus::message::object_path isolateHardware,
        sdbusplus::xyz::openbmc_project::HardwareIsolation::server::Entry::Type
            severity) override;

    /**
     *  @brief Implementation for CreateWithErrorLog
     *
     *  @param[in] isolateHardware - The hardware inventory path which is needs
     * to isolate.
     *  @param[in] severity - The severity of isolating hardware.
     *  @param[in] bmcErrorLog - The BMC error log caused the isolation of
     * hardware.
     *
     *  @return path The path of created
     * xyz.openbmc_project.HardwareIsolation.Entry object.
     */
    sdbusplus::message::object_path createWithErrorLog(
        sdbusplus::message::object_path isolateHardware,
        sdbusplus::xyz::openbmc_project::HardwareIsolation::server::Entry::Type
            severity,
        sdbusplus::message::object_path bmcErrorLog) override;

    /**
     * @brief Delete all isolated hardware entires
     *
     * @return NULL
     */
    void deleteAll() override;

    /**
     * @brief Create dbus objects for isolated hardwares
     *        from their persisted location.
     *
     * return NULL on success.
     *        Throw exception on failure.
     */
    void restore();

    /**
     * @brief Callback to add the dbus entry for host isolated hardwares.
     */
    void handleHostIsolatedHardwares();

    /**
     * @brief Implementation for CreateWithEntityPath
     *
     * @param[in] entityPath - The hardware entity path which is needs
     *                         to isolate
     * @param[in] severity - The severity of isolating hardware.
     * @param[in] bmcErrorLog - The BMC error log caused the isolation of
     *                          hardware.
     *
     * @return path The path of created
     *              xyz.openbmc_project.HardwareIsolation.Entry object.
     */
    sdbusplus::message::object_path createWithEntityPath(
        std::vector<uint8_t> entityPath,
        sdbusplus::xyz::openbmc_project::HardwareIsolation::server::Entry::Type
            severity,
        sdbusplus::message::object_path bmcErrorLog) override;

  private:
    /**
     *  * @brief Attached bus connection
     *   */
    sdbusplus::bus::bus& _bus;

    /**
     * @brief Last created entry id
     */
    entry::EntryId _lastEntryId;

    /**
     * @brief Isolated hardwares list
     */
    IsolatedHardwares _isolatedHardwares;

    /**
     * @brief Used to get isolatable hardware details
     */
    isolatable_hws::IsolatableHWs _isolatableHWs;

    /**
     * @brief Watcher to add dbus entry for host isolated hardware
     */
    watch::inotify::Watch _guardFileWatch;

    /**
     * @brief Used to get EID (aka PEL ID) by using BMC log
     *        object path
     *
     * @param[in] bmcErrorLog - The BMC error log dbus object path to
     *                          get EID
     *
     * @return EID (aka PEL ID) on success
     *         Empty optional on failure
     */
    std::optional<uint32_t>
        getEID(const sdbusplus::message::object_path& bmcErrorLog) const;

    /**
     * @brief Used to set the Available property value by using given
     *        dbus object path
     *
     * @param[in] dbusObjPath - The object path to set available property value
     * @param[in] availablePropVal - set the available property value
     *
     * @return NULL on success
     *         Throw exception on failure
     *
     * @note It will set available property value if found the available
     *       property in the given object path. If not found then it will just
     *       add the trace and won't throw exception.
     */
    void setAvailableProperty(const std::string& dbusObjPath,
                              bool availablePropVal);

    /**
     * @brief Create a entry dbus object for isolated hardware
     *
     * @param[in] recordId - the isolated hardware record id
     * @param[in] resolved - resolved status of isolated hardware
     * @param[in] severity - the severity of hardware isolation
     * @param[in] isolatedHardware - the isolated hardware object path
     * @param[in] errorLogId - The error log which caused the hardware isolation
     * @param[in] deleteRecord - delete record if failed to create entry
     * @param[in] entityPath - the isolated hardware entity path
     *
     * @return entry object path on success
     *         Empty optional on failure
     */
    std::optional<sdbusplus::message::object_path>
        createEntry(const entry::EntryRecordId& recordId,
                    const entry::EntryResolved& resolved,
                    const entry::EntrySeverity& severity,
                    const std::string& isolatedHardware,
                    const std::string& bmcErrorLog, const bool deleteRecord,
                    const openpower_guard::EntityPath& entityPath);

    /**
     * @brief Used to get to know whether hardware isolation is allowed
     *
     * @param[in] severity - the severity of hardware isolation
     *
     * @return Throw appropriate exception if not allowed
     *         NULL if allowed
     */
    void isHwIsolationAllowed(const entry::EntrySeverity& severity);

    /**
     * @brief Used to get BMC log object path by using EID (aka PEL ID)
     *
     * @param[in] eid - The EID (aka PEL ID) to get BMC log object path
     *
     * @return The BMC log object path
     *         Empty optional on failure
     */
    std::optional<sdbusplus::message::object_path>
        getBMCLogPath(const uint32_t eid) const;

    /**
     * @brief Create dbus entry object for isolated hardware record
     *
     * @param[in] record - The isolated hardware record
     *
     * @return NULL on success
     *
     * @note The function will skip given isolated hardware to create
     *       dbus entry if any failure since this is restoring mechanism
     *       so the hardware isolation application need to create dbus entries
     *       for all isolated hardware that is stored in the preserved location.
     */
    void createEntryForRecord(const openpower_guard::GuardRecord& record);
};

} // namespace hw_isolation
