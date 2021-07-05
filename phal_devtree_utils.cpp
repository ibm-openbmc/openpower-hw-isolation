// SPDX-License-Identifier: Apache-2.0

extern "C"
{
#include <libpdbg.h>
}

#include "config.h"

#include "attributes_info.H"

#include "phal_devtree_utils.hpp"

#include <fmt/format.h>
#include <stdlib.h>

#include <phosphor-logging/elog-errors.hpp>

#include <stdexcept>

namespace hw_isolation
{
namespace devtree
{

using namespace phosphor::logging;

void initPHAL()
{
    // Set PDBG_DTB environment variable to use interested phal cec device tree
    if (setenv("PDBG_DTB", PHAL_DEVTREE, 1))
    {
        log<level::ERR>(
            fmt::format(
                "Failed to set PDBG_DTB with ErrNo [{}] and ErrMsg [{}]", errno,
                strerror(errno))
                .c_str());
        throw std::runtime_error(
            "Failed to set PDBG_DTB while trying to init PHAL");
    }

    // Set log level to info
    pdbg_set_loglevel(PDBG_ERROR);

    /**
     * Passing fdt argument as NULL so, pdbg will use PDBG_DTB environment
     * variable to get interested phal cec device tree instead of default pdbg
     * device tree.
     */
    if (!pdbg_targets_init(NULL))
    {
        throw std::runtime_error("pdbg target initialization failed");
    }
}

namespace lookup_func
{
CanGetPhysPath mruId(struct pdbg_target* pdbgTgt, InstanceId instanceId,
                     LocationCode locCode)
{
    CanGetPhysPath canGetPhysPath = false;

    ATTR_MRU_ID_Type devTreeMruId;
    if (DT_GET_PROP(ATTR_MRU_ID, pdbgTgt, devTreeMruId))
    {
        throw std::runtime_error(
            std::string("Failed to get ATTR_MRU_ID from ") +
            pdbg_target_path(pdbgTgt));
    }

    // Last two byte (from MSB) of MRU_ID having instance number
    if ((devTreeMruId & 0xFFFF) == instanceId)
    {
        canGetPhysPath = true;
    }

    // If given target having location attribute then check that with given
    // location code.
    ATTR_LOCATION_CODE_Type devTreelocCode;
    if (!DT_GET_PROP(ATTR_LOCATION_CODE, pdbgTgt, devTreelocCode) &&
        (canGetPhysPath == true))
    {
        // If location code did not match then given device tree target is not
        // expected one.
        if (LocationCode(devTreelocCode) != locCode)
        {
            canGetPhysPath = false;
        }
    }

    return canGetPhysPath;
}

CanGetPhysPath chipUnitPos(struct pdbg_target* pdbgTgt, InstanceId instanceId,
                           LocationCode locCode)
{
    CanGetPhysPath canGetPhysPath = false;

    ATTR_CHIP_UNIT_POS_Type devTreeChipUnitPos;
    if (DT_GET_PROP(ATTR_CHIP_UNIT_POS, pdbgTgt, devTreeChipUnitPos))
    {
        throw std::runtime_error(
            std::string("Failed to get ATTR_CHIP_UNIT_POS from ") +
            pdbg_target_path(pdbgTgt));
    }
    if (devTreeChipUnitPos == instanceId)
    {
        canGetPhysPath = true;
    }
    return canGetPhysPath;
}

CanGetPhysPath locationCode(struct pdbg_target* pdbgTgt, InstanceId instanceId,
                            LocationCode locCode)
{
    CanGetPhysPath canGetPhysPath = false;

    ATTR_LOCATION_CODE_Type devTreelocCode;
    if (DT_GET_PROP(ATTR_LOCATION_CODE, pdbgTgt, devTreelocCode))
    {
        throw std::runtime_error(
            std::string("Failed to get ATTR_LOCATION_CODE from ") +
            pdbg_target_path(pdbgTgt));
    }

    if (LocationCode(devTreelocCode) == locCode)
    {
        canGetPhysPath = true;
    }
    return canGetPhysPath;
}

} // namespace lookup_func

} // namespace devtree
} // namespace hw_isolation
