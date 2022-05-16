// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common/common_types.hpp"
#include "common/isolatable_hardwares.hpp"
#include "common/watch.hpp"
#include "hw_isolation_record/entry.hpp"
#include "hw_isolation_record/openpower_guard_interface.hpp"
#include "org/open_power/HardwareIsolation/Create/server.hpp"
#include "xyz/openbmc_project/Collection/DeleteAll/server.hpp"
#include "xyz/openbmc_project/HardwareIsolation/Create/server.hpp"

#include <cereal/types/set.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>

#include <queue>

namespace hw_isolation
{
namespace record
{

using CreateInterface =
    sdbusplus::xyz::openbmc_project::HardwareIsolation::server::Create;
using OP_CreateInterface =
    sdbusplus::org::open_power::HardwareIsolation::server::Create;

using IsolatedHardwares =
    std::map<entry::EntryRecordId, std::unique_ptr<entry::Entry>>;

using DeleteAllInterface =
    sdbusplus::xyz::openbmc_project::Collection::server::DeleteAll;

using EcoCores = std::set<devtree::DevTreePhysPath>;

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
            const sdeventplus::Event& eventLoop);

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
     * @brief Erase the entry from the manager
     *
     * @param[in] entryRecordId - The entry record id to erase
     *
     * @return NULL
     */
    void eraseEntry(const entry::EntryRecordId entryRecordId);

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
     * @brief Callback to process hardware isolation record file
     *
     * @return NULL
     */
    void processHardwareIsolationRecordFile();

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

    /**
     * @brief Used to the isolated hardware entry information.
     *
     * @param[in] hwInventoryPath - the hardware inventory path to get
     *                              the entry information.
     *
     * @return tuple with EntrySeverity, EntryErrLogPath on success
     *         Empty optional if the hardware is not isolated
     *
     * @note The EntryErrLogPath will be empty if the respective entry
     *       does not have bmc error log path.
     */
    std::optional<std::tuple<entry::EntrySeverity, entry::EntryErrLogPath>>
        getIsolatedHwRecordInfo(
            const sdbusplus::message::object_path& hwInventoryPath);

  private:
    /**
     *  * @brief Attached bus connection
     *   */
    sdbusplus::bus::bus& _bus;

    /**
     * @brief Attached sd_event loop
     */
    const sdeventplus::Event& _eventLoop;

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
     * @brief Timer to wake and process hardware isolation record file
     */
    std::queue<std::unique_ptr<
        sdeventplus::utility::Timer<sdeventplus::ClockId::Monotonic>>>
        _timerObjs;

    /**
     * @brief Used to maintain isolated eco core records.
     *
     * @details * TODO: It is a workaround until fix the following issue
     *            ibm-openbmc/dev/issues/3573.
     *          * It will only persisted to use in the disruptive code update
     *            restore path.
     */
    EcoCores _persistedEcoCores;

    /**
     * @brief Allow cereal class access to allow save and load functions
     *        to be private
     */
    friend class cereal::access;

    /**
     * @brief Helper template that is required by Cereal to perform
     *        serialization and deserialization.
     *
     * @details * TODO: It is a workaround until fix the following issue
     *            ibm-openbmc/dev/issues/3573.
     *          * It will only serialize and deserialize
     *            the "_persistedEcoCores" member that is not persisted
     *            in the disruptive code update.
     *
     * @tparam Archive    - Cereal archive type.
     * @param[in] archive - Reference to Cereal archive.
     * @param[in] version - Class version that enables handling
     *                      a serialized data.
     *
     * @return NULL
     */
    template <class Archive>
    void serialize(Archive& archive, const uint32_t /*version*/)
    {
        archive(_persistedEcoCores);
    }

    /**
     * @brief Used to serialize ECO core records.
     *
     * @return NULL
     */
    void serialize();

    /**
     * @brief Used to Deserialize ECO core records.
     *
     * @return true if deserialized false otherwise.
     */
    bool deserialize();

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
     * @brief Create a entry dbus object for isolated hardware
     *
     * @param[in] recordId - the isolated hardware record id
     * @param[in] resolved - resolved status of isolated hardware
     * @param[in] severity - the severity of hardware isolation
     * @param[in] isolatedHardware - the isolated hardware object path
     * @param[in] bmcErrorLog - The error log which caused the hardware
     *                          isolation
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
     * @brief Update a entry dbus object for isolated hardware if exists
     *
     * @param[in] recordId - the isolated hardware record id
     * @param[in] severity - the severity of hardware isolation
     * @param[in] isolatedHwDbusObjPath - the isolated hardware D-Bus object
     *                                    path
     * @param[in] bmcErrorLog - The error log which caused the hardware
     *                          isolation
     * @param[in] entityPath - the isolated hardware entity path
     *
     * @return pair<true, object_path> on success
     *         pair<false, ""> on failure
     */
    std::pair<bool, sdbusplus::message::object_path>
        updateEntry(const entry::EntryRecordId& recordId,
                    const entry::EntrySeverity& severity,
                    const std::string& isolatedHwDbusObjPath,
                    const std::string& bmcErrorLog,
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
     * @brief Create dbus entry object for isolated hardware record
     *
     * @param[in] record - The isolated hardware record
     * @param[in] isRestorePath - Used to indicate whether trying get inventory
     *                            path at the restore path or runtime.
     *                            By default is "false".
     *
     * @return NULL on success
     *
     * @note The function will skip given isolated hardware to create
     *       dbus entry if any failure since this is restoring mechanism
     *       so the hardware isolation application need to create dbus entries
     *       for all isolated hardware that is stored in the preserved location.
     */
    void createEntryForRecord(const openpower_guard::GuardRecord& record,
                              const bool isRestorePath = false);

    /**
     * @brief Update the given dbus entry object for isolated hardware record
     *
     * @param[in] record - The isolated hardware record
     * @param[out] entryIt - The dbus entry object to update
     *
     * @return NULL on success
     *
     * @note The function will skip given isolated hardware to update
     *       dbus entry if any failure since this is restoring mechanism
     *       so the hardware isolation application need to update dbus entries
     *       for all isolated hardware that is stored in the preserved location.
     */
    void updateEntryForRecord(const openpower_guard::GuardRecord& record,
                              IsolatedHardwares::iterator& entryIt);

    /**
     * @brief Callback to add the dbus entry for host isolated hardwares.
     *
     * @return NULL
     */
    void handleHostIsolatedHardwares();

    /**
     * @brief Resolve all entries.
     *
     * @param[in] clearRecord - use to decide whether want to clear
     *                          record from their preserved file.
     *                          By default, it will clear record.
     * @return NULL
     *
     * @note This function just resolve the entries, won't check
     *       whether resolve operation is allowed or not like "deleteAll()".
     */
    void resolveAllEntries(bool clearRecord = true);

    /**
     * @brief Helper API to check whether hardware isolation record
     *        is valid or not.
     *
     * @param[in] recordId - The record id to check
     *
     * @return true if the given record id is valid else false
     */
    bool isValidRecord(const entry::EntryRecordId recordId);

    /**
     * @brief Helper API to cleanup persisted files
     *
     * @return NULL
     */
    void cleanupPersistedFiles();

    /**
     * @brief Helper API to update ECO cores list based on the given
     *        "ecoCore" flag.
     *
     * @param[in] ecoCore - Indicate whether the Core is eco core or not.
     * @param[in] coreDevTreePhysPath - The core device tree physical path.
     *
     * @return NULL
     */
    void
        updateEcoCoresList(const bool ecoCore,
                           const devtree::DevTreePhysPath& coreDevTreePhysPath);

    /**
     * @brief Helper API to cleanup persisted eco cores
     *
     * @return NULL
     */
    void cleanupPersistedEcoCores();
};

} // namespace record
} // namespace hw_isolation
