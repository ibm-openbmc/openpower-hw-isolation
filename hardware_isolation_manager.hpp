// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common_types.hpp"
#include "hardware_isolation_entry.hpp"

namespace hw_isolation
{

using IsolatedHardwares =
    std::map<entry::EntryId, std::unique_ptr<entry::Entry>>;

/**
 *  @class Manager
 *  @brief Hardware isolation manager implementation
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
     *  @param[in] bus - Bus to attach to.
     */
    Manager(sdbusplus::bus::bus& bus);

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
     *
     * @return entry object path on success
     *         Empty optional on failure
     */
    std::optional<sdbusplus::message::object_path>
        createEntry(const entry::EntryRecordId& recordId,
                    const entry::EntryResolved& resolved,
                    const entry::EntrySeverity& severity,
                    const std::string& isolatedHardware,
                    const std::string& bmcErrorLog, const bool deleteRecord);
};
} // namespace hw_isolation
