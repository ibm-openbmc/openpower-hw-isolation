// SPDX-License-Identifier: Apache-2.0

#pragma once

extern "C"
{
#include <libpdbg.h>
}

#include "common_types.hpp"
#include "openpower_guard_interface.hpp"

#include <functional>
#include <optional>
#include <string>

namespace hw_isolation
{
namespace devtree
{

using namespace hw_isolation::type;
using DevTreePhysPath = std::vector<uint8_t>;

/**
 * @brief API to init PHAL (POWER Hardware Abstraction Layer)
 *
 * @details PHAL should init to use phal cec device tree
 */
void initPHAL();

/**
 * @brief Get unexpanded location code
 *
 * @details An api to get an unexpanded location code corresponding to
 *          a given expanded location code. Unexpanded location codes
 *          give the location of the FRU in the system.
 *
 * @param[in] locCode - Location code in expanded format.
 *
 * @return Location code as string which will be in un-expanded format or
 *         empty optional on failure.
 */
std::optional<LocationCode> getUnexpandedLocCode(const std::string& locCode);

/**
 * @brief Used to get physical path of isolate hardware
 *        phal cec device tree
 *
 * @param[in] isolateHw - pdbg target of isolate hardware
 *
 * @return DevTreePhysPath on success
 *         Throw exception on failure
 */
DevTreePhysPath getPhysicalPath(struct pdbg_target* isolateHw);

/**
 * @brief Used to get phal cec device tree target based on
 *        the given hardware physical path which is isolated
 *
 * @param[in] physicalPath - the hardware physical path to get
 *                           the phal cec device tree target
 *
 * @return The phal cec device tree target on success
 *         Empty optional on failure
 */
std::optional<struct pdbg_target*>
    getPhalDevTreeTgt(const DevTreePhysPath& physicalPath);

/**
 * @brief Used to get the FRU information (location code and instance id)
 *        from the phal cec device tree by using phal cec device tree target
 *
 * @param[in] fruTgt - The FRU target to get its information
 *
 * @return pair<LocationCode, InstanceId> on success
 *         Throw exception on failure
 */
std::pair<LocationCode, InstanceId> getFRUDetails(struct pdbg_target* fruTgt);

/**
 * @brief Used to get the instance id of the given phal cec device tree target
 *
 * @param[in] devTreeTgt - The phal cec device tree target to get instance id
 *
 * @return InstanceId on success
 *         Throw exception on failure
 */
InstanceId getHwInstIdFromDevTree(struct pdbg_target* devTreeTgt);

/**
 * @brief Helper function to get raw data from openpower Entity path
 *
 * @param[in] entityPath - The hardware entity path to get raw data
 *
 * @return DevTreePhysPath on success
 *         Throw exception on failure
 */
DevTreePhysPath
    convertEntityPathIntoRawData(const openpower_guard::EntityPath& entityPath);

/**
 * @brief Used to add functions that will use to get to know
 *        whether the given target (aka device tree node) can
 *        be used to get the physical path of expected hardware.
 */
namespace lookup_func
{

using CanGetPhysPath = bool;

/**
 * @brief Lookup function signature
 *
 * @param[in] pdbg_target - phal cec device tree target (node).
 * @param[in] InstanceId - instance id of hardware to check with
 *                         phal cec device tree.
 * @param[in] LocationCode - location code of hardware to check
 *                           with phal cec device tree.
 *                           This is optional if hardware is non-fru.
 *
 * @return CanGetPhysPath to indicate whether can get the physical path
 *         or not from given phal cec device tree target.
 *
 * @note All lookup functions which are added in this namespace should
 *       match with below signature.
 */
using LookupFuncForPhysPath = std::function<CanGetPhysPath(
    struct pdbg_target*, InstanceId, LocationCode)>;

CanGetPhysPath mruId(struct pdbg_target* pdbgTgt, InstanceId instanceId,
                     LocationCode locCode);

CanGetPhysPath chipUnitPos(struct pdbg_target* pdbgTgt, InstanceId instanceId,
                           LocationCode locCode);

CanGetPhysPath locationCode(struct pdbg_target* pdbgTgt, InstanceId instanceId,
                            LocationCode locCode);

} // namespace lookup_func
} // namespace  devtree
} // namespace hw_isolation
