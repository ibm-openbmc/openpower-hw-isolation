#include <attributes_info.H>
#include <libphal.H>

#include <deconfig_reason.hpp>
#include <deconfig_records.hpp>
#include <libguard/guard_interface.hpp>
#include <phosphor-logging/lg2.hpp>
#include <util.hpp>
namespace openpower::faultlog
{

using ::nlohmann::json;

constexpr auto stateConfigured = "CONFIGURED";
constexpr auto stateDeconfigured = "DECONFIGURED";

/**
 * @brief Get pdbg targets which has been deconfigured
 *
 * This callback function is called as part of the recursive method
 * pdbg_target_traverse, recursion will exit when the method return 1
 * else continues till the target is found
 *
 * @param[in] target - pdbg target to compare
 * @param[inout] priv - data structure to pass to the callback method
 *
 * @return 1 when target is found else 0
 */
static int getDeconfigTargets(struct pdbg_target* target, void* priv)
{
    auto deconfigList = reinterpret_cast<DeconfigDataList*>(priv);
    ATTR_HWAS_STATE_Type hwasState;
    if (!DT_GET_PROP(ATTR_HWAS_STATE, target, hwasState))
    {
        if ((DECONFIGURED_BY_PLID_MASK & hwasState.deconfiguredByEid) == 0)
        {
            // inlcude only specific states and other might be by association
            switch (hwasState.deconfiguredByEid)
            {
                case DECONFIGURED_BY_MANUAL_GARD:
                case DECONFIGURED_BY_FIELD_CORE_OVERRIDE:
                case DECONFIGURED_BY_PRD:
                case DECONFIGURED_BY_PHYP:
                case DECONFIGURED_BY_SPCN:
                {
                    deconfigList->addPdbgTarget(target);
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
        else if (hwasState.deconfiguredByEid != 0)
        {
            deconfigList->addPdbgTarget(target);
        }
    }
    return 0;
}

DeconfigDataList
    DeconfigRecords::getDeconfigList(const GuardRecords& guardRecords)
{
    std::vector<std::string> pathList;
    for (const auto& elem : guardRecords)
    {
        if (elem.errType ==
            static_cast<uint8_t>(openpower::guard::GardType::GARD_Spare))
        {
            // if guarded due to spare ignore it
            continue;
        }
        auto physicalPath = openpower::guard::getPhysicalPath(elem.targetId);
        if (!physicalPath.has_value())
        {
            continue;
        }
        pathList.push_back(*physicalPath);
    }

    DeconfigDataList deconfigList;
    pdbg_target_traverse(nullptr, getDeconfigTargets, &deconfigList);
    DeconfigDataList onlyDeconfigList;
    for (const auto& target : deconfigList.targetList)
    {
        ATTR_PHYS_DEV_PATH_Type attrPhyDevPath;
        if (!DT_GET_PROP(ATTR_PHYS_DEV_PATH, target, attrPhyDevPath))
        {
            std::string phyPathStr(attrPhyDevPath, sizeof(attrPhyDevPath));
            // consider only those targets that are not part of guard list
            if (std::find(pathList.begin(), pathList.end(), phyPathStr) ==
                pathList.end())
            {
                onlyDeconfigList.addPdbgTarget(target);
            }
        }
    }
    return onlyDeconfigList;
}

int DeconfigRecords::getCount(const GuardRecords& guardRecords)
{
    return static_cast<int>(getDeconfigList(guardRecords).targetList.size());
}

void DeconfigRecords::populate(const GuardRecords& guardRecords,
                               nlohmann::json& jsonNag)
{
    DeconfigDataList onlyDeconfigList = getDeconfigList(guardRecords);

    for (const auto& target : onlyDeconfigList.targetList)
    {
        try
        {
            json deconfigJson = json::object();
            deconfigJson["TYPE"] = pdbgTargetName(target);
            std::string state = stateDeconfigured;
            ATTR_HWAS_STATE_Type hwasState;
            if (!DT_GET_PROP(ATTR_HWAS_STATE, target, hwasState))
            {
                if (hwasState.functional)
                {
                    state = stateConfigured;
                }
                deconfigJson["PLID"] = 0x0;
                if ((DECONFIGURED_BY_PLID_MASK & hwasState.deconfiguredByEid) !=
                    0)
                {
                    std::stringstream ss;
                    ss << std::hex << "0x" << hwasState.deconfiguredByEid;
                    deconfigJson["PLID"] = ss.str();
                }
                deconfigJson["REASON_DESCRIPTION"] =
                    getDeconfigReason(static_cast<DeconfiguredByReason>(
                        hwasState.deconfiguredByEid));
            }
            deconfigJson["CURRENT_STATE"] = std::move(state);

            ATTR_PHYS_DEV_PATH_Type attrPhyDevPath;
            if (!DT_GET_PROP(ATTR_PHYS_DEV_PATH, target, attrPhyDevPath))
            {
                deconfigJson["PHYS_PATH"] = attrPhyDevPath;
            }
            else
            {
                // if physical path is not found do not add the record
                // as it will be of no use
                continue;
            }

            // getLocationCode checks if attr is present in target else
            // gets it from partent target
            ATTR_LOCATION_CODE_Type attrLocCode = {'\0'};
            openpower::phal::pdbg::getLocationCode(target, attrLocCode);
            deconfigJson["LOCATION_CODE"] = attrLocCode;

            json header = json::object();
            header["DECONFIGURED"] = std::move(deconfigJson);
            jsonNag.push_back(std::move(header));
        }
        catch (const std::exception& ex)
        {
            lg2::error("Failed to add deconfig records {TARGET} {ERROR}",
                       "TARGET", pdbgTargetName(target), "ERROR", ex.what());
        }
    }
}
} // namespace openpower::faultlog
