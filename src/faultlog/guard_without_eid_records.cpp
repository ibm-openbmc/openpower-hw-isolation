#include <attributes_info.H>
#include <libphal.H>

#include <guard_without_eid_records.hpp>
#include <libguard/guard_interface.hpp>
#include <phosphor-logging/lg2.hpp>
#include <poweron_time.hpp>
#include <util.hpp>
extern "C"
{
#include <libpdbg.h>
}
namespace openpower::faultlog
{

using ::nlohmann::json;

constexpr auto stateConfigured = "CONFIGURED";
constexpr auto stateDeconfigured = "DECONFIGURED";

/** Data structure to pass to pdbg_target_traverse callback method*/
struct GuardedWithoutEidDataList
{
    std::vector<pdbg_target*> targetList;
    std::vector<std::string> pathList;
    void addPhysicalPath(const std::string& path)
    {
        pathList.emplace_back(path);
    }
    void addPdbgTarget(pdbg_target* tgt)
    {
        targetList.push_back(tgt);
    }
};

/**
 * @brief Get PDBG targets matching the phtysical path list
 *
 * This callback function is called as part of the recursive method
 * pdbg_target_traverse, recursion will exit when the method return 1
 * else continues till the target is found
 *
 * @param[in] target - pdbg target to compare
 d* @param[inout] priv - data structure to pass to the callback method
 *
 * @return 0 when all the targets are parsed
 */
static int guardedTargets(struct pdbg_target* target, void* priv)
{
    // Parse through all the device tree targets recursively and
    // check if the physical path of the target matches any of the
    // deconfigured targets physical path if so store the target
    // in the list.
    //
    // Avoiding multiple traversal through the device tree targets
    GuardedWithoutEidDataList* deconfig =
        reinterpret_cast<GuardedWithoutEidDataList*>(priv);
    ATTR_PHYS_DEV_PATH_Type phyPath;
    if (!DT_GET_PROP(ATTR_PHYS_DEV_PATH, target, phyPath))
    {
        for (auto& path : deconfig->pathList)
        {
            if (strcmp(phyPath, path.c_str()) == 0)
            {
                deconfig->addPdbgTarget(target);
            }
        }
        if (deconfig->pathList.size() == deconfig->targetList.size())
        {
            return 1;
        }
    }
    return 0;
}

int GuardWithoutEidRecords::getCount(const GuardRecords& guardRecords)
{
    int count = 0;
    for (const auto& elem : guardRecords)
    {
        if (elem.elogId != 0)
        {
            // only cater guards without a PEL
            continue;
        }
        if (elem.errType ==
            static_cast<uint8_t>(openpower::guard::GardType::GARD_Spare))
        {
            // if guarded due to spare ignore it
            continue;
        }
        auto physicalPath = openpower::guard::getPhysicalPath(elem.targetId);
        if (!physicalPath.has_value())
        {
            lg2::error("Failed to get physical path for record {RECORD_ID}",
                       "RECORD_ID", elem.recordId);
            continue;
        }
        count += 1;
    }
    return count;
}

void GuardWithoutEidRecords::populate(const GuardRecords& guardRecords,
                                      nlohmann::json& jsonNag)
{
    try
    {
        // capure the physical path of all the isolated/guard records
        // that does not have an errorlog object created. Those with
        // corresponding errorlog object are covered in ServiceableRecords
        GuardedWithoutEidDataList guardList;
        for (const auto& elem : guardRecords)
        {
            if (elem.elogId != 0)
            {
                // only cater guards without a PEL
                continue;
            }
            if (elem.errType ==
                static_cast<uint8_t>(openpower::guard::GardType::GARD_Spare))
            {
                // if guarded due to spare ignore it
                continue;
            }
            auto physicalPath =
                openpower::guard::getPhysicalPath(elem.targetId);
            if (!physicalPath.has_value())
            {
                lg2::error("Failed to get physical path for record {RECORD_ID}",
                           "RECORD_ID", elem.recordId);
                continue;
            }
            guardList.addPhysicalPath(*physicalPath);
        }

        // traverse through all targets and get guarded targets list
        pdbg_target_traverse(nullptr, guardedTargets, &guardList);

        for (const auto& target : guardList.targetList)
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
            }
            deconfigJson["CURRENT_STATE"] = std::move(state);

            ATTR_PHYS_DEV_PATH_Type attrPhyDevPath;
            if (!DT_GET_PROP(ATTR_PHYS_DEV_PATH, target, attrPhyDevPath))
            {
                deconfigJson["PHYS_PATH"] = attrPhyDevPath;
                deconfigJson["REASON_DESCRIPTION"] =
                    getGuardReason(guardRecords, attrPhyDevPath);
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
            header["MANUAL_ISOLATION"] = std::move(deconfigJson);
            jsonNag.push_back(std::move(header));
        }
    }
    catch (const std::exception& ex)
    {
        lg2::error("Failed to add manual guard records {ERROR}", "ERROR",
                   ex.what());
    }
}
} // namespace openpower::faultlog
