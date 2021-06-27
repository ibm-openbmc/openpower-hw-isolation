// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common_types.hpp"

#include <xyz/openbmc_project/HardwareIsolation/Entry/server.hpp>

#include <string>

namespace hw_isolation
{
namespace entry
{

using EntryInterface =
    sdbusplus::xyz::openbmc_project::HardwareIsolation::server::Entry;
using EntryId = uint32_t;
using EntryRecordId = uint32_t;
using EntrySeverity =
    sdbusplus::xyz::openbmc_project::HardwareIsolation::server::Entry::Type;
using EntryResolved = bool;

/**
 * @class Entry
 *
 * @brief Hardware isolation entry implementation
 *
 * @details Implemented the below interfaces
 *          xyz.openbmc_project.HardwareIsolation.Entry
 *
 */
class Entry : public type::ServerObject<EntryInterface>
{
  public:
    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
    virtual ~Entry() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *
     *  @param[in] bus - Bus to attach with dbus entry object path.
     *  @param[in] path - Entry dbus object path to attach.
     *  @param[in] entryId - the dbus entry id.
     *  @param[in] entryRecordId - the isolated hardware record id.
     *  @param[in] isolatedHwSeverity - the severity hardware isolation.
     *  @param[in] entryIsResolved - the status of hardware isolation.
     */
    Entry(sdbusplus::bus::bus& bus, const std::string& objPath,
          const EntryId entryId, const EntryRecordId entryRecordId,
          const EntrySeverity isolatedHwSeverity,
          const EntryResolved entryIsResolved);

  private:
    /** @brief The id of isolated hardware dbus entry */
    EntryId _entryId;

    /** @brief The record id of isolated hardware dbus entry
     *
     *  @note This record id is shared between BMC and Host applications.
     */
    EntryRecordId _entryRecordId;

}; // end of Entry class

} // namespace entry
} // namespace hw_isolation
