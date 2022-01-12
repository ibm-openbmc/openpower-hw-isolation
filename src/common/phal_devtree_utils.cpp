// SPDX-License-Identifier: Apache-2.0

extern "C"
{
#include <libpdbg.h>
}

#include "config.h"

#include "attributes_info.H"

#include "common/phal_devtree_utils.hpp"

#include <fmt/format.h>
#include <stdlib.h>

#include <phosphor-logging/elog-errors.hpp>

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace hw_isolation
{
namespace devtree
{
using namespace phosphor::logging;

/**
 * Used to return in pdbg callback function.
 * The value for constexpr is defined based on pdbg_target_traverse function
 * usage.
 */
constexpr int continueTgtTraversal = 0;
constexpr int requireIsolateHwFound = 1;

struct CecDevTreeHw
{
    ATTR_PHYS_BIN_PATH_Type physBinPath;

    struct pdbg_target* reqDevTreeHw;

    CecDevTreeHw()
    {
        std::memset(&physBinPath, 0, sizeof(physBinPath));
        reqDevTreeHw = nullptr;
    }
};

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

/**
 * @brief pdbg callback to identify a target based on the given
 *        attribute
 *
 * @param[in] target current device tree target
 * @param[in|out] userData for accessing|storing from|to user
 *
 * @return 0 to continue traverse, non-zero to stop traverse
 */
int pdbgCallbackToGetTgt(struct pdbg_target* target, void* userData)
{
    CecDevTreeHw* cecDevTreeHw = static_cast<CecDevTreeHw*>(userData);

    /**
     * The use case is, find the target from the cec device tree based on the
     * given attribute value. So, don't use "DT_GET_PROP" to read attribute
     * because it will add trace if the given attribute is not found to read.
     */
    ATTR_PHYS_BIN_PATH_Type physBinPath;
    if (!pdbg_target_get_attribute(
            target, "ATTR_PHYS_BIN_PATH",
            std::stoi(dtAttr::fapi2::ATTR_PHYS_BIN_PATH_Spec),
            dtAttr::fapi2::ATTR_PHYS_BIN_PATH_ElementCount, physBinPath))
    {
        return continueTgtTraversal;
    }

    if (std::memcmp(physBinPath, cecDevTreeHw->physBinPath,
                    sizeof(physBinPath)) != 0)
    {
        return continueTgtTraversal;
    }

    // Found the required cec device tree target (hardware)
    cecDevTreeHw->reqDevTreeHw = target;

    return requireIsolateHwFound;
}

std::optional<struct pdbg_target*>
    getPhalDevTreeTgt(const DevTreePhysPath& physicalPath)
{
    CecDevTreeHw cecDevTreeHw;

    size_t physBinPathSize = sizeof(cecDevTreeHw.physBinPath);
    if (physBinPathSize < physicalPath.size())
    {
        log<level::ERR>(fmt::format("EntityPath size is mismatch. "
                                    " Given size [{}] and Expected size [{}]",
                                    physicalPath.size(), physBinPathSize)
                            .c_str());
        return std::nullopt;
    }
    std::copy(physicalPath.begin(), physicalPath.end(),
              cecDevTreeHw.physBinPath);

    auto ret = pdbg_target_traverse(NULL, pdbgCallbackToGetTgt, &cecDevTreeHw);

    if (ret != requireIsolateHwFound)
    {
        std::stringstream ss;
        std::for_each(physicalPath.begin(), physicalPath.end(),
                      [&ss](const auto& ele) {
                          ss << std::setw(2) << std::setfill('0') << std::hex
                             << (int)ele << " ";
                      });

        log<level::ERR>(fmt::format("Isolated HW [{}] is "
                                    "not found in the cec device tree",
                                    ss.str())
                            .c_str());
        return std::nullopt;
    }

    return cecDevTreeHw.reqDevTreeHw;
}

std::pair<LocationCode, InstanceId> getFRUDetails(struct pdbg_target* fruTgt)
{
    ATTR_LOCATION_CODE_Type frulocCode;
    if (DT_GET_PROP(ATTR_LOCATION_CODE, fruTgt, frulocCode))
    {
        throw std::runtime_error(
            std::string("Failed to get ATTR_LOCATION_CODE from ") +
            pdbg_target_path(fruTgt));
    }

    InstanceId instanceId{type::Invalid_InstId};
    ATTR_MRU_ID_Type mruId;
    /**
     * The use case is, get mru id if present in the FRU target
     * so don't use "DT_GET_PROP" to read attribute because it will add trace
     * if the given attribute is not found.
     *
     * For example, DIMM doesn't have MRU_ID.
     */
    if (pdbg_target_get_attribute(
            fruTgt, "ATTR_MRU_ID", std::stoi(dtAttr::fapi2::ATTR_MRU_ID_Spec),
            dtAttr::fapi2::ATTR_MRU_ID_ElementCount, &mruId))
    {
        // Last two byte (from MSB) of MRU_ID having instance number
        instanceId = mruId & 0xFFFF;
    }

    return std::make_pair(LocationCode(frulocCode), instanceId);
}

InstanceId getHwInstIdFromDevTree(struct pdbg_target* devTreeTgt)
{
    bool isChipletUnit = false;

    ATTR_CHIPLET_ID_Type chipletId;
    if (!DT_GET_PROP(ATTR_CHIPLET_ID, devTreeTgt, chipletId))
    {
        if (chipletId != 0xFF)
        {
            isChipletUnit = true;
        }
    }

    InstanceId instanceId;
    // Given hardware unit is packaged inside chiplet
    if (isChipletUnit)
    {
        /**
         * FIXME: FC target does not contain ATTR_CHIP_UNIT_POS attribute
         *        since it is logical unit and pub-ekb attribute xml file
         *        does not have TARGET_TYPE_FC for ATTR_CHIP_UNIT_POS.
         *        so using pdbg index until it get added into phal
         *        device tree.
         */
        if (std::string(pdbg_target_class_name(devTreeTgt)) == "fc")
        {
            instanceId = pdbg_target_index(devTreeTgt);
        }
        else
        {
            ATTR_CHIP_UNIT_POS_Type devTreeChipUnitPos;
            if (DT_GET_PROP(ATTR_CHIP_UNIT_POS, devTreeTgt, devTreeChipUnitPos))
            {
                throw std::runtime_error(
                    std::string("Failed to get ATTR_CHIP_UNIT_POS from ") +
                    pdbg_target_path(devTreeTgt));
            }
            instanceId = devTreeChipUnitPos;
        }
    }
    else
    {
        /**
         * Check If MRU_ID is present. If yes, use it else use pdbg target index
         * Example: The MRU_ID is present for nx which is not a chiplet
         *
         * Don't use "DT_GET_PROP" to read attribute because it will add trace
         * if the given attribute is not found.
         */
        ATTR_MRU_ID_Type devTreeMruId;
        if (pdbg_target_get_attribute(
                devTreeTgt, "ATTR_MRU_ID",
                std::stoi(dtAttr::fapi2::ATTR_MRU_ID_Spec),
                dtAttr::fapi2::ATTR_MRU_ID_ElementCount, &devTreeMruId))
        {
            // Last two byte (from MSB) of MRU_ID having instance number
            instanceId = devTreeMruId & 0xFFFF;
        }
        else
        {
            instanceId = pdbg_target_index(devTreeTgt);
        }
    }

    return instanceId;
}

DevTreePhysPath
    convertEntityPathIntoRawData(const openpower_guard::EntityPath& entityPath)
{
    DevTreePhysPath rawData;
    rawData.push_back(entityPath.type_size);

    /**
     * Path elements size stored at last 4bits in type_size member.
     * For every iteration, increasing 2 byte to store PathElement value
     * i.e target type enum value and instance id as raw data.
     *
     * @note PathElement targetType and instance member data type are
     * uint8_t. so, directly assigning each value as one byte to make
     * raw data (binary) format. If data types are changed for those fields
     * in PathElements then this logic also needs to revisit.
     */
    for (int i = 0; i < (0x0F & entityPath.type_size); i++)
    {
        rawData.push_back(entityPath.pathElements[i].targetType);
        rawData.push_back(entityPath.pathElements[i].instance);
    }
    return rawData;
}

bool isECOcore(struct pdbg_target* coreTgt)
{
    ATTR_ECO_MODE_Type ecoMode;
    if (DT_GET_PROP(ATTR_ECO_MODE, coreTgt, ecoMode))
    {
        log<level::ERR>(
            fmt::format(
                "Failed to get ATTR_ECO_MODE from the given core target [{}]",
                pdbg_target_path(coreTgt))
                .c_str());
        return false;
    }

    if (ecoMode == ENUM_ATTR_ECO_MODE_ENABLED)
    {
        return true;
    }
    else
    {
        return false;
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
                           LocationCode /* locCode */)
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

CanGetPhysPath locationCode(struct pdbg_target* pdbgTgt,
                            InstanceId /* instanceId */, LocationCode locCode)
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

CanGetPhysPath pdbgIndex(struct pdbg_target* pdbgTgt, InstanceId instanceId,
                         LocationCode /* locCode */)
{
    return pdbg_target_index(pdbgTgt) == instanceId;
}

} // namespace lookup_func

} // namespace devtree
} // namespace hw_isolation
