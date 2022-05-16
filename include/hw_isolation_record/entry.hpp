// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common/common_types.hpp"
#include "common/phal_devtree_utils.hpp"
#include "hw_isolation_record/openpower_guard_interface.hpp"

#include <cereal/access.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/HardwareIsolation/Entry/server.hpp>
#include <xyz/openbmc_project/Object/Delete/server.hpp>
#include <xyz/openbmc_project/Time/EpochTime/server.hpp>

#include <tuple>
#include <vector>

namespace hw_isolation
{
namespace record
{

class Manager;

constexpr auto HW_ISOLATION_ENTRY_PERSIST_PATH =
    "/var/lib/op-hw-isolation/persistdata/record_entry/{}";

namespace entry
{

using EntryInterface =
    sdbusplus::xyz::openbmc_project::HardwareIsolation::server::Entry;
using EntryRecordId = uint32_t;
using EntrySeverity =
    sdbusplus::xyz::openbmc_project::HardwareIsolation::server::Entry::Type;
using EntryResolved = bool;

using AssociationDefInterface =
    sdbusplus::xyz::openbmc_project::Association::server::Definitions;

using EpochTime = sdbusplus::xyz::openbmc_project::Time::server::EpochTime;

using DeleteInterface = sdbusplus::xyz::openbmc_project::Object::server::Delete;

using EntryErrLogPath = std::string;

/**
 * @class Entry
 *
 * @brief Hardware isolation entry implementation
 *
 * @details Implemented the below interfaces
 *          xyz.openbmc_project.HardwareIsolation.Entry
 *          xyz.openbmc_project.Association.Definitions
 *          xyz.openbmc_project.Time.EpochTime
 *          xyz.openbmc_project.Object.Delete
 *
 */
class Entry :
    public type::ServerObject<EntryInterface, AssociationDefInterface,
                              EpochTime, DeleteInterface>
{
  public:
    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
    virtual ~Entry();

    /** @brief Constructor to put object onto bus at a dbus path.
     *
     *  @param[in] bus - Bus to attach with dbus entry object path.
     *  @param[in] objPath - Entry dbus object path to attach.
     *  @param[in] hwIsolationRecordMgr - The manager who owns entry.
     *  @param[in] entryRecordId - the isolated hardware record id.
     *  @param[in] isolatedHwSeverity - the severity hardware isolation.
     *  @param[in] entryIsResolved - the status of hardware isolation.
     *  @param[in] associationDef - the association to hold other dbus
     *                              object path along with entry object.
     *  @param[in] entityPath - the entry entity path of hardware
     */
    Entry(sdbusplus::bus::bus& bus, const std::string& objPath,
          hw_isolation::record::Manager& hwIsolationRecordMgr,
          const EntryRecordId entryRecordId,
          const EntrySeverity isolatedHwSeverity,
          const EntryResolved entryIsResolved,
          const type::AssociationDef& associationDef,
          const openpower_guard::EntityPath& entityPath);

    /**
     * @brief Mark this object as resolved
     *
     * @param[in] clearRecord - use to decide whether want to clear
     *                          record from their preserved file.
     *                          By default, it will clear record.
     * @return NULL
     *
     * @note This function just resolve the entry, won't check
     *       whether resolve operation is allowed or not like "delete_()".
     */
    void resolveEntry(bool clearRecord = true);

    /**
     *  @brief Mark this object as resolved
     *
     *  @return NULL
     */
    void delete_() override;

    /**
     * @brief Used get the entity path of isolated hardware.
     */
    openpower_guard::EntityPath getEntityPath() const;

    /**
     * @brief Used get the record id of isolated hardware.
     */
    EntryRecordId getEntryRecId() const;

    /**
     * @brief Serialize and persisted the required members
     *
     * @return NULL
     */
    void serialize();

    /**
     * @brief Deserialize the persisted members.
     *
     * @return true if deserialized false otherwise.
     */
    bool deserialize();

  private:
    /** @brief Attached bus connection */
    sdbusplus::bus::bus& _bus;

    /** @brief The Manager who owns this enty */
    hw_isolation::record::Manager& _hwIsolationRecordMgr;

    /** @brief The record id of isolated hardware dbus entry
     *
     *  @note This record id is shared between BMC and Host applications.
     */
    EntryRecordId _entryRecordId;

    /** @brief The entity path of this entry */
    openpower_guard::EntityPath _entityPath;

    /**
     * @brief Allow cereal class access to allow save and load functions
     *        to be private
     */
    friend class cereal::access;

    /**
     * @brief Helper template that is required by Cereal to perform
     *        serialization.
     *
     * @details It will only serialize the required members that are not
     *          persisted in the hardware isolation partition file that
     *          shared between the BMC and Host applications.
     *
     * @tparam Archive    - Cereal archive type (BinaryOutputArchive).
     * @param[in] archive - Reference to Cereal archive.
     * @param[in] version - Class version that enables handling
     *                      a serialized data.
     *
     * @return NULL
     */
    template <class Archive>
    void save(Archive& archive, const uint32_t /*version*/) const
    {
        auto entityPathRawData =
            hw_isolation::devtree::convertEntityPathIntoRawData(_entityPath);

        archive(entityPathRawData, elapsed());
    }

    /**
     * @brief Helper template that is required by Cereal to perform
     *        deserialization.
     *
     * @details It will only deserialize the required members that are not
     *          persisted in the hardware isolation partition file that
     *          shared between the BMC and Host applications.
     *
     * @tparam Archive    - Cereal archive type (BinaryInputArchive).
     * @param[in] archive - Reference to Cereal archive.
     * @param[in] version - Class version that enables handling
     *                      a deserialized data.
     *
     * @return NULL
     */
    template <class Archive>
    void load(Archive& archive, const uint32_t /*version*/)
    {
        hw_isolation::devtree::DevTreePhysPath persistedEntityPathRawData;
        uint64_t persistedElapsed;

        // Must be ordered based on the serialization to deserialize.
        archive(persistedEntityPathRawData, persistedElapsed);

        if (openpower_guard::EntityPath(persistedEntityPathRawData.data()) ==
            _entityPath)
        {
            // Skip to send property change signal in the restore path.
            elapsed(persistedElapsed, true);
        }
        else
        {
            serialize();
        }
    }

}; // end of Entry class

namespace utils
{

/**
 * @brief Helper function to get EntrySeverity based on
 *        the given GardType
 *
 * @param[in] gardType openpower gard type
 *
 * @return EntrySeverity on success
 *         Empty optional on failure
 *
 * @note This function will return EntrySeverity::Warning
 * if the given GardType is not found in conversion switch block
 */
std::optional<EntrySeverity>
    getEntrySeverityType(const openpower_guard::GardType gardType);

/**
 * @breif Helper function to get the GardType based on
 *        the given EntrySeverity
 *
 * @param[in] severity hardware isloation entry severity
 *
 * @return GardType on success
 *         Empty optional on failure
 *
 * @note This function will return GardType::GARD_Predictive
 * if the given EntrySeverity is not found in conversion switch block
 *
 */
std::optional<openpower_guard::GardType>
    getGuardType(const EntrySeverity severity);

} // namespace utils
} // namespace entry
} // namespace record
} // namespace hw_isolation
