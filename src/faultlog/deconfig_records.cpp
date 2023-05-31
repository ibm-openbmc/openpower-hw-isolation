#include <attributes_info.H>

#include <deconfig_reason.hpp>
#include <deconfig_records.hpp>
#include <phosphor-logging/lg2.hpp>

extern "C"
{
#include <libpdbg.h>
}
namespace openpower::faultlog
{

using ::nlohmann::json;

constexpr auto stateConfigured = "CONFIGURED";
constexpr auto stateDeconfigured = "DECONFIGURED";

struct DeconfigDataList
{
    std::vector<pdbg_target*> targetList;
    void addPdbgTarget(pdbg_target* tgt)
    {
        targetList.push_back(tgt);
    }
};

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
        if ((hwasState.deconfiguredByEid != 0) &&
            (DECONFIGURED_BY_PLID_MASK & hwasState.deconfiguredByEid) == 0)
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
                    lg2::info(
                        "Ignoring deconfig target {TARGET} {DECONFIG_EID} ",
                        "TARGET", pdbg_target_name(target), "DECONFIG_EID",
                        static_cast<int>(hwasState.deconfiguredByEid));
                    break;
                }
            }
        }
    }
    return 0;
}

int DeconfigRecords::getCount()
{
    DeconfigDataList deconfigList;
    pdbg_target_traverse(nullptr, getDeconfigTargets, &deconfigList);
    return static_cast<int>(deconfigList.targetList.size());
}

void DeconfigRecords::populate(nlohmann::json& jsonNag)
{
    DeconfigDataList deconfigList;
    pdbg_target_traverse(nullptr, getDeconfigTargets, &deconfigList);

    for (auto target : deconfigList.targetList)
    {
        try
        {
            json deconfigJson = json::object();
            deconfigJson["TYPE"] = pdbg_target_name(target);
            std::string state = stateDeconfigured;
            ATTR_HWAS_STATE_Type hwasState;
            if (!DT_GET_PROP(ATTR_HWAS_STATE, target, hwasState))
            {
                if (hwasState.functional)
                {
                    state = stateConfigured;
                }
            }
            deconfigJson["CURRENT_STATE"] = std::move(state);

            ATTR_PHYS_DEV_PATH_Type attrPhyDevPath;
            if (!DT_GET_PROP(ATTR_PHYS_DEV_PATH, target, attrPhyDevPath))
            {
                deconfigJson["PHYS_PATH"] = attrPhyDevPath;
            }

            ATTR_LOCATION_CODE_Type attrLocCode;
            if (!DT_GET_PROP(ATTR_LOCATION_CODE, target, attrLocCode))
            {
                deconfigJson["LOCATION_CODE"] = attrLocCode;
            }

            std::stringstream ss;
            ss << "0x" << std::hex << hwasState.deconfiguredByEid;
            deconfigJson["PLID"] = ss.str();
            deconfigJson["REASON_DESCRIPTION"] = getDeconfigReason(
                static_cast<DeconfiguredByReason>(hwasState.deconfiguredByEid));

            json header = json::object();
            header["DECONFIGURED"] = std::move(deconfigJson);
            jsonNag.push_back(std::move(header));
        }
        catch (const std::exception& ex)
        {
            lg2::error("Failed to add deconfig records {TARGET} {ERROR}",
                       "TARGET", pdbg_target_name(target), "ERROR", ex.what());
        }
    }
}
} // namespace openpower::faultlog
