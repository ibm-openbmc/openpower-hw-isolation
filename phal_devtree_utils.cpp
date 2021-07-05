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

std::optional<LocationCode> getUnexpandedLocCode(const std::string& locCode)
{
    // Location code should start with "U"
    if (locCode[0] != 'U')
    {
        log<level::ERR>(fmt::format("Location code should start with \"U\""
                                    " but, given location code [{}]",
                                    locCode)
                            .c_str());
        return std::nullopt;
    }

    // Given location code length should be need to match with minimum length
    // to drop expanded format in given location code.
    constexpr uint8_t EXP_LOCATIN_CODE_MIN_LENGTH = 17;
    if (locCode.length() < EXP_LOCATIN_CODE_MIN_LENGTH)
    {
        log<level::ERR>(fmt::format("Given location code [{}] is not meet "
                                    "with minimum length [{}]",
                                    locCode, EXP_LOCATIN_CODE_MIN_LENGTH)
                            .c_str());
        return std::nullopt;
    }

    // "-" should be there to seggregate all (FC, Node number and SE values)
    // together.
    // Note: Each (FC, Node number and SE) value can be seggregate by "."
    // but, cec device tree just have unexpand format so, just skipping
    // still first occurrence "-" and replacing with "fcs".
    auto endPosOfFcs = locCode.find('-', EXP_LOCATIN_CODE_MIN_LENGTH);
    if (endPosOfFcs == std::string::npos)
    {
        log<level::ERR>(fmt::format("Given location code [{}] is not "
                                    "valid i.e could not find dash",
                                    locCode)
                            .c_str());
        return std::nullopt;
    }

    std::string unExpandedLocCode("Ufcs");

    if (locCode.length() > EXP_LOCATIN_CODE_MIN_LENGTH)
    {
        unExpandedLocCode.append(
            locCode.substr(endPosOfFcs, std::string::npos));
    }

    return unExpandedLocCode;
}

DevTreePhysPath getPhysicalPath(struct pdbg_target* isolateHw)
{
    ATTR_PHYS_BIN_PATH_Type physPath;
    if (DT_GET_PROP(ATTR_PHYS_BIN_PATH, isolateHw, physPath))
    {
        throw std::runtime_error(
            std::string("Failed to get ATTR_PHYS_BIN_PATH") +
            pdbg_target_path(isolateHw));
    }
    return DevTreePhysPath(physPath,
                           physPath + sizeof(physPath) / sizeof(physPath[0]));
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
