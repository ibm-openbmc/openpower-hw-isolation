// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <libguard/guard_interface.hpp>

namespace hw_isolation
{
namespace openpower_guard
{

namespace libguard = openpower::guard;
using GardType = libguard::GardType;
using GuardRecord = libguard::GuardRecord;
using EntityPath = libguard::EntityPath;

/**
 * @brief Wrapper function for libguard::create to create guard record
 *        into partition
 *
 * @param[in] entityPath - the hardware path which needs to isolate
 * @param[in] errorLogId - The error log id (aka EID) caused the isolation
 *                         of hardware
 * @param[in] guardType - The guard type of hardware isolation
 *
 * @return GuardRecord on success
 *         Throw exception on failure
 */
std::optional<GuardRecord> create(const EntityPath& entityPath,
                                  const uint32_t errorLogId,
                                  const GardType guardType);

/**
 * @brief Wrapper function for libguard::clear to delete guard record
 *        by using record id
 *
 * @param[in] recordId - The guard record id to delete from partition
 *
 * @return NULL on success
 *         Throw exception on failure
 */
void clear(const uint32_t recordId);

} // namespace openpower_guard
} // namespace hw_isolation
