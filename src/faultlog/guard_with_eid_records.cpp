#include <attributes_info.H>
#include <libphal.H>

#include <guard_with_eid_records.hpp>
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
struct GuardedTarget
{
    pdbg_target* target = nullptr;
    std::string phyDevPath = {};
    GuardedTarget(const std::string& path) : phyDevPath(path) {}
};

using PropertyValue =
    std::variant<std::string, bool, uint8_t, int16_t, uint16_t, int32_t,
                 uint32_t, int64_t, uint64_t, double>;

using Properties = std::map<std::string, PropertyValue>;
/**
 * @brief Get PDBG target matching the guarded target physicalpath
 *
 * This callback function is called as part of the recursive method
 * pdbg_target_traverse, recursion will exit when the method return 1
 * else continues till the target is found
 *
 * @param[in] target - pdbg target to compare
 * @param[inout] priv - data structure passed to the callback method
 *
 * @return 1 when target is found else 0
 */
static int getGuardedTarget(struct pdbg_target* target, void* priv)
{
    GuardedTarget* guardTarget = reinterpret_cast<GuardedTarget*>(priv);
    ATTR_PHYS_DEV_PATH_Type phyPath;
    if (!DT_GET_PROP(ATTR_PHYS_DEV_PATH, target, phyPath))
    {
        if (strcmp(phyPath, guardTarget->phyDevPath.c_str()) == 0)
        {
            guardTarget->target = target;
            return 1;
        }
    }
    return 0;
}

int GuardWithEidRecords::getCount(sdbusplus::bus::bus& bus,
                                  const GuardRecords& guardRecords)
{
    // An error could create single PEL but multiple guard records, while
    // processing guard records do not create multiple error log sections
    // as the callout data retrieved from the PEL will be the same, rather
    // add only resource actions which differs and is based on guarded target.
    // 0x00000001 | 0x89007371 | predictive      |
    //  physical:sys-0/node-0/proc-2/mc-0/mi-0/mcc-1/omi-1
    // 0x00000003 | 0x89007371 | predictive      |
    //  physical:sys-0/node-0/ocmb_chip-19
    std::vector<uint32_t> liProcessedPels;
    for (const auto& elem : guardRecords)
    {
        // ignore manual guard records
        if (elem.elogId == 0)
        {
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

        GuardedTarget guardedTarget(*physicalPath);
        pdbg_target_traverse(nullptr, getGuardedTarget, &guardedTarget);
        if (guardedTarget.target == nullptr)
        {
            lg2::error("Failed to find the pdbg target for the guarded "
                       "target {RECORD_ID}",
                       "RECORD_ID", elem.recordId);
            continue;
        }
        ATTR_HWAS_STATE_Type hwasState;
        if (DT_GET_PROP(ATTR_HWAS_STATE, guardedTarget.target, hwasState))
        {
            lg2::error("Failed to get HWAS state of the guarded "
                       "target {RECORD_ID}",
                       "RECORD_ID", elem.recordId);
            continue;
        }
        bool dbusErrorObjFound = true;
        uint32_t bmcLogId = 0;
        try
        {
            auto method = bus.new_method_call(
                "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
                "org.open_power.Logging.PEL", "GetBMCLogIdFromPELId");

            method.append(static_cast<uint32_t>(elem.elogId));
            auto resp = bus.call(method);
            resp.read(bmcLogId);
        }
        catch (const sdbusplus::exception::SdBusError& ex)
        {
            dbusErrorObjFound = false;
            lg2::info(
                "PEL might be deleted but guard entry is around {ELOG_ID)",
                "ELOG_ID", elem.elogId);
        }
        uint32_t plid = 0;
        if (dbusErrorObjFound)
        {
            Properties pelEntryProp;
            std::string objPath = "/xyz/openbmc_project/logging/entry/" +
                                  std::to_string(bmcLogId);
            auto pelEntryMethod = bus.new_method_call(
                "xyz.openbmc_project.Logging", objPath.c_str(),
                "org.freedesktop.DBus.Properties", "GetAll");
            pelEntryMethod.append("org.open_power.Logging.PEL.Entry");
            auto pelEntryReply = bus.call(pelEntryMethod);
            pelEntryReply.read(pelEntryProp);
            for (const auto& [prop, propValue] : pelEntryProp)
            {
                if (prop == "PlatformLogID")
                {
                    auto plidPtr = std::get_if<uint32_t>(&propValue);
                    if (plidPtr != nullptr)
                    {
                        plid = *plidPtr;
                    }
                }
            }
        }
        // D-Bus error object might be deleted
        else
        {
            // hwas state will be updated only during reipl till then plid will
            // be zero, if zero assume it as new serviceable event else check if
            // it is already processed
            plid = static_cast<uint32_t>(hwasState.deconfiguredByEid);
        }

        // plid could be zero if pel is deleted so do not ignore those guard
        // records
        if (plid != 0)
        {
            // A single PEL could capture multiple guard records callouts so
            // ignore a guard record if already cpatured as part of a PEL
            if (std::find(liProcessedPels.begin(), liProcessedPels.end(),
                          plid) != liProcessedPels.end())
            {
                lg2::info("Ignoring PEL as it has been already processed with "
                          "another guard record {PEL_ID}",
                          "PEL_ID", plid);
                continue;
            }
        }
        liProcessedPels.push_back(plid);
    }
    return liProcessedPels.size();
}

void GuardWithEidRecords::populate(sdbusplus::bus::bus& bus,
                                   const GuardRecords& guardRecords,
                                   json& jsonServEvent)
{
    // to allow duplicte pels that are deleted which will have plid as zero till
    // reipl
    std::multimap<uint32_t, json> processedPelMap;

    for (const auto& elem : guardRecords)
    {
        try
        {
            // ignore manual guard records
            if (elem.elogId == 0)
            {
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

            GuardedTarget guardedTarget(*physicalPath);
            pdbg_target_traverse(nullptr, getGuardedTarget, &guardedTarget);
            if (guardedTarget.target == nullptr)
            {
                lg2::error("Failed to find the pdbg target for the guarded "
                           "target {RECORD_ID}",
                           "RECORD_ID", elem.recordId);
                continue;
            }
            ATTR_HWAS_STATE_Type hwasState;
            if (DT_GET_PROP(ATTR_HWAS_STATE, guardedTarget.target, hwasState))
            {
                lg2::error("Failed to get HWAS state of the guarded "
                           "target {RECORD_ID}",
                           "RECORD_ID", elem.recordId);
                continue;
            }
            uint32_t bmcLogId = 0;
            uint32_t plid = 0;
            json jsonErrorLog = json::object();
            bool dbusErrorObjFound = true;
            try
            {
                auto method = bus.new_method_call(
                    "xyz.openbmc_project.Logging",
                    "/xyz/openbmc_project/logging",
                    "org.open_power.Logging.PEL", "GetBMCLogIdFromPELId");

                method.append(static_cast<uint32_t>(elem.elogId));
                auto resp = bus.call(method);
                resp.read(bmcLogId);
            }
            catch (const sdbusplus::exception::SdBusError& ex)
            {
                dbusErrorObjFound = false;
                lg2::info(
                    "PEL might be deleted but guard entry is around {ELOG_ID}",
                    "ELOG_ID", elem.elogId);
            }
            if (dbusErrorObjFound)
            {
                std::string callouts;
                std::string refCode;
                std::string objPath = "/xyz/openbmc_project/logging/entry/" +
                                      std::to_string(bmcLogId);
                Properties loggingEntryProp;
                auto loggingEntryMethod = bus.new_method_call(
                    "xyz.openbmc_project.Logging", objPath.c_str(),
                    "org.freedesktop.DBus.Properties", "GetAll");
                loggingEntryMethod.append("xyz.openbmc_project.Logging.Entry");
                auto loggingEntryReply = bus.call(loggingEntryMethod);
                loggingEntryReply.read(loggingEntryProp);
                for (const auto& [prop, propValue] : loggingEntryProp)
                {
                    if (prop == "Resolution")
                    {
                        auto calloutsPtr = std::get_if<std::string>(&propValue);
                        if (calloutsPtr != nullptr)
                        {
                            callouts = *calloutsPtr;
                        }
                    }
                    else if (prop == "EventId")
                    {
                        auto eventIdPtr = std::get_if<std::string>(&propValue);
                        if (eventIdPtr != nullptr)
                        {
                            std::istringstream iss(*eventIdPtr);
                            iss >> refCode;
                        }
                    }
                } // endfor
                uint64_t timestamp = 0;
                Properties pelEntryProp;
                auto pelEntryMethod = bus.new_method_call(
                    "xyz.openbmc_project.Logging", objPath.c_str(),
                    "org.freedesktop.DBus.Properties", "GetAll");
                pelEntryMethod.append("org.open_power.Logging.PEL.Entry");
                auto pelEntryReply = bus.call(pelEntryMethod);
                pelEntryReply.read(pelEntryProp);
                for (const auto& [prop, propValue] : pelEntryProp)
                {
                    if (prop == "PlatformLogID")
                    {
                        auto plidPtr = std::get_if<uint32_t>(&propValue);
                        if (plidPtr != nullptr)
                        {
                            plid = *plidPtr;
                        }
                    }
                    else if (prop == "Timestamp")
                    {
                        auto timestampPtr = std::get_if<uint64_t>(&propValue);
                        if (timestampPtr != nullptr)
                        {
                            timestamp = *timestampPtr;
                        }
                    }
                }
                std::stringstream ss;
                ss << std::hex << "0x" << plid;
                jsonErrorLog["PLID"] = ss.str();
                jsonErrorLog["Callout Section"] = parseCallout(callouts);
                jsonErrorLog["SRC"] = refCode;
                jsonErrorLog["DATE_TIME"] = epochTimeToBCD(timestamp);
            }
            else
            {
                json jsonCallout = json::object();
                json sectionJson = json::object();

                // getLocationCode checks if attr is present in target else
                // gets it from partent target
                ATTR_LOCATION_CODE_Type attrLocCode = {'\0'};
                openpower::phal::pdbg::getLocationCode(guardedTarget.target,
                                                       attrLocCode);
                jsonCallout["Location Code"] = attrLocCode;

                sectionJson["Callout Count"] = 1;
                sectionJson["Callouts"] = jsonCallout;
                std::stringstream ss;
                ss << std::hex << "0x" << hwasState.deconfiguredByEid;
                jsonErrorLog["PLID"] = ss.str();
                plid = hwasState.deconfiguredByEid;
                jsonErrorLog["Callout Section"] = sectionJson;
                jsonErrorLog["SRC"] = 0;
                jsonErrorLog["DATE_TIME"] = "00/00/0000 00:00:00";
            }

            // populate resource actions section
            json jsonResource = json::object();
            jsonResource["TYPE"] = pdbgTargetName(guardedTarget.target);
            std::string state = stateDeconfigured;
            if (hwasState.functional)
            {
                state = stateConfigured;
            }
            jsonResource["CURRENT_STATE"] = std::move(state);

            jsonResource["REASON_DESCRIPTION"] = getGuardReason(guardRecords,
                                                                *physicalPath);

            jsonResource["GUARD_RECORD"] = true;
            ATTR_PHYS_DEV_PATH_Type phyPath;
            if (!DT_GET_PROP(ATTR_PHYS_DEV_PATH, guardedTarget.target, phyPath))
            {
                jsonResource["PHYS_PATH"] = phyPath;
            }
            // An error could create single PEL but multiple guard records,
            // while processing guard records do not create multiple error log
            // sections as the callout data retrieved from the PEL will be the
            // same, rather add only resource actions which differs and is based
            // on guarded target. 0x00000001 | 0x89007371 | predictive      |
            //      physical:sys-0/node-0/proc-2/mc-0/mi-0/mcc-1/omi-1
            // 0x00000003 | 0x89007371 | predictive      |
            //      physical:sys-0/node-0/ocmb_chip-19

            // if PEL is deleted plid will be zero till reipl, for them
            // consider it as new cec_error_log
            bool fNewSection = true;
            if (plid != 0)
            {
                auto it = processedPelMap.find(plid);
                if (it != processedPelMap.end())
                {
                    // add resource actions to existing errorlog section
                    lg2::info("Ignoring PEL callout data as it is already "
                              "processed with {PEL_ID}",
                              "PEL_ID", plid);
                    json jsonResActions = json::object();
                    jsonResActions["RESOURCE_ACTIONS"] =
                        std::move(jsonResource);
                    it->second.push_back(jsonResActions);
                    fNewSection = false;
                }
            }
            // create new errorlog section if plid is zero or if it is
            // not processed already
            if (fNewSection)
            {
                json jsonErrorLogSection = json::array();
                jsonErrorLogSection.push_back(std::move(jsonErrorLog));

                json jsonResActions = json::object();
                jsonResActions["RESOURCE_ACTIONS"] = std::move(jsonResource);
                jsonErrorLogSection.push_back(jsonResActions);

                processedPelMap.emplace(plid, jsonErrorLogSection);
            }
        }
        catch (const std::exception& ex)
        {
            lg2::info("Failed to add guard record {ELOG_ID}, {ERROR}",
                      "ELOG_ID", elem.elogId, "ERROR", ex);
        }
    }

    // add all the cecerrroglog json objects to the servicable event json object
    for (const auto& pair : processedPelMap)
    {
        json jsonErrlogObj = json::object();
        jsonErrlogObj["CEC_ERROR_LOG"] = std::move(pair.second);
        jsonServEvent.emplace_back(jsonErrlogObj);
    }
}
} // namespace openpower::faultlog
