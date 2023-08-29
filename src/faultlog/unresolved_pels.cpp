#include <attributes_info.H>

#include <libguard/guard_interface.hpp>
#include <phosphor-logging/log.hpp>
#include <unresolved_pels.hpp>
#include <util.hpp>

extern "C"
{
#include <libpdbg.h>
}

namespace openpower::faultlog
{
using ::nlohmann::json;

using PropertyValue =
    std::variant<std::string, bool, uint8_t, int16_t, uint16_t, int32_t,
                 uint32_t, int64_t, uint64_t, double>;

using Properties = std::map<std::string, PropertyValue>;

using Interfaces = std::map<std::string, Properties>;

using Objects = std::map<sdbusplus::message::object_path, Interfaces>;

constexpr auto stateConfigured = "CONFIGURED";
constexpr auto stateDeconfigured = "DECONFIGURED";
constexpr std::string pwrThermalErrPrefix = "1100";
constexpr auto chassisPwnOnStartedErrSrc = "BD8D3416";

struct GuardedTarget
{
    pdbg_target* target = nullptr;
    std::string phyDevPath;
    GuardedTarget(const std::string& path) : phyDevPath(path)
    {}
};

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
    // recursive callback function that exits when the target matching the
    // guarded targets physical path is found in the device tree.
    // to exit the recursive function return 1 to continue return 0
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

/**
 * @brief Return timestamp of the ChassisPowerOnStarted PEL
 *
 * Loop through all the D-Bus error objects to find PEL matching
 * ChassisPowerOnStarted and return the timestamp of it.
 *
 * For faultlog consider only those pels that are logged after chassis
 * has powered-on i.e after xyz.openbmc_project.State.Info.ChassisPowerOnStarted
 * PEL has been created. If poweron PEL is not found consider
 * all errors. This is done to remove duplicate PELS that are
 * logged during IPL for a long running system. Considering only latest
 * errors.
 *
 * For some of the PELS user might not have changed the resolved bit
 * to true after replacing a failed FRU, it too will show up in the
 * faultlog dump so considering only those that are logged after poweron.
 *
 * @param[in] objects - error log D-Bus objects
 *
 * @return timestamp of the pel if found else return 0
 */
static uint64_t getChassisPoweronErrTimestamp(const Objects& objects)
{
    // xyz.openbmc_project.State.Info.ChassisPowerOnStarted pel
    uint64_t timestamp = 0;
    uint64_t max_timestamp = 0;
    std::string refCode;
    for (const auto& [path, interfaces] : objects)
    {
        for (const auto& [intf, properties] : interfaces)
        {
            if (intf == "xyz.openbmc_project.Logging.Entry")
            {
                for (const auto& [prop, propValue] : properties)
                {
                    if (prop == "EventId")
                    {
                        auto eventIdPtr = std::get_if<std::string>(&propValue);
                        if (eventIdPtr != nullptr)
                        {
                            // EventId B700900B 00000072 00010016 ...
                            // First value is RefCode
                            std::istringstream iss(*eventIdPtr);
                            iss >> refCode;
                        }
                    }
                }
            }
            else if (intf == "org.open_power.Logging.PEL.Entry")
            {
                for (const auto& [prop, propValue] : properties)
                {
                    if (prop == "Timestamp")
                    {
                        auto timestampPtr = std::get_if<uint64_t>(&propValue);
                        if (timestampPtr != nullptr)
                        {
                            timestamp = *timestampPtr;
                        }
                    }
                }
            }
        }

        // if chassis poweron src is found return the timestamp
        // of that pel or error object
        if (refCode == chassisPwnOnStartedErrSrc)
        {
            if (max_timestamp < timestamp)
            {
                max_timestamp = timestamp;
            }
        }
    }

    lg2::info("Latest chassis poweron time stamp is :{TIME}", "TIME",
              epochTimeToBCD(max_timestamp));
    return max_timestamp;
}

int UnresolvedPELs::getCount(sdbusplus::bus::bus& bus, bool ignorePwrFanPel)
{
    int count = 0;
    try
    {
        Objects objects;
        auto method = bus.new_method_call(
            "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
        auto reply = bus.call(method);
        reply.read(objects);

        // read timestamp of poweron PEL
        uint64_t poweronTimestamp = getChassisPoweronErrTimestamp(objects);
        // Ignore PELS if chassis poweron pel is not found
        if (poweronTimestamp == 0)
        {
            lg2::info(
                "Ignoring error PELs as chassis poweron pel is not found ");
            return count;
        }

        for (const auto& [path, interfaces] : objects)
        {
            bool resolved = true;
            std::string severity =
                "xyz.openbmc_project.Logging.Entry.Level.Informational";
            bool deconfigured = false;
            bool guarded = false;
            std::string refCode;
            uint64_t timestamp = 0;
            for (const auto& [intf, properties] : interfaces)
            {
                if (intf == "xyz.openbmc_project.Logging.Entry")
                {
                    for (const auto& [prop, propValue] : properties)
                    {
                        if (prop == "Resolved")
                        {
                            auto resolvedPtr = std::get_if<bool>(&propValue);
                            if (resolvedPtr != nullptr)
                            {
                                resolved = *resolvedPtr;
                            }
                        }
                        else if (prop == "Severity")
                        {
                            auto severityPtr =
                                std::get_if<std::string>(&propValue);
                            if (severityPtr != nullptr)
                            {
                                severity = *severityPtr;
                            }
                        }
                        else if (prop == "EventId")
                        {
                            auto eventIdPtr =
                                std::get_if<std::string>(&propValue);
                            if (eventIdPtr != nullptr)
                            {
                                // EventId B700900B 00000072 00010016 ...
                                // First value is RefCode
                                std::istringstream iss(*eventIdPtr);
                                iss >> refCode;
                            }
                        }
                    }
                }
                else if (intf == "org.open_power.Logging.PEL.Entry")
                {
                    for (const auto& [prop, propValue] : properties)
                    {
                        if (prop == "Deconfig")
                        {
                            auto deconfigPtr = std::get_if<bool>(&propValue);
                            if (deconfigPtr != nullptr)
                            {
                                deconfigured = *deconfigPtr;
                            }
                        }
                        else if (prop == "Guard")
                        {
                            auto guardPtr = std::get_if<bool>(&propValue);
                            if (guardPtr != nullptr)
                            {
                                guarded = *guardPtr;
                            }
                        }
                        else if (prop == "Timestamp")
                        {
                            auto timestampPtr =
                                std::get_if<uint64_t>(&propValue);
                            if (timestampPtr != nullptr)
                            {
                                timestamp = *timestampPtr;
                            }
                        }
                    }
                }
            }

            if (resolved == true)
            {
                continue;
            }

            if (ignorePwrFanPel)
            {
                // power and thermal err src starts with 1100
                if (refCode.substr(0, pwrThermalErrPrefix.length()) ==
                    pwrThermalErrPrefix)
                {
                    continue;
                }
            }
            // ignore informational and debug errors
            if ((severity == "xyz.openbmc_project.Logging.Entry.Level.Debug") ||
                (severity ==
                 "xyz.openbmc_project.Logging.Entry.Level.Informational") ||
                (severity == "xyz.openbmc_project.Logging.Entry.Level.Notice"))
            {
                continue;
            }

            if (deconfigured == false)
            {
                continue;
            }

            if (guarded == true) // will be captured as part of guard records
            {
                continue;
            }
            // Ignore PELS that are created before chassis poweron
            if (timestamp < poweronTimestamp)
            {
                continue;
            }
            count += 1;
        } // endfor
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        lg2::info("There are no PELS or PEL corresponding to guard record is "
                  "deleted "
                  " {ERROR}",
                  "ERROR", ex);
    }
    catch (const std::exception& ex)
    {
        lg2::error("Failed to get count of unresolved pels with deconfig bit "
                   "set {ERROR}",
                   "ERROR", ex);
    }
    return count;
}

void UnresolvedPELs::populate(sdbusplus::bus::bus& bus,
                              const GuardRecords& guardRecords, json& jsonNag)
{
    try
    {
        Objects objects;
        auto method = bus.new_method_call(
            "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
        auto reply = bus.call(method);
        reply.read(objects);

        uint64_t poweronTimestamp = getChassisPoweronErrTimestamp(objects);
        // Ignore PELS if chassis poweron pel is not found
        if (poweronTimestamp == 0)
        {
            lg2::info(
                "Ignoring error PELs as chassis poweron pel is not found ");
            return;
        }

        for (const auto& [path, interfaces] : objects)
        {
            bool resolved = true;
            std::string severity =
                "xyz.openbmc_project.Logging.Entry.Level.Informational";
            uint32_t plid = 0;
            bool deconfigured = false;
            bool guarded = false;
            uint64_t timestamp = 0;
            std::string callouts;
            std::string refCode;
            for (const auto& [intf, properties] : interfaces)
            {
                if (intf == "xyz.openbmc_project.Logging.Entry")
                {
                    for (const auto& [prop, propValue] : properties)
                    {
                        if (prop == "Resolved")
                        {
                            auto resolvedPtr = std::get_if<bool>(&propValue);
                            if (resolvedPtr != nullptr)
                            {
                                resolved = *resolvedPtr;
                            }
                        }
                        else if (prop == "Severity")
                        {
                            auto severityPtr =
                                std::get_if<std::string>(&propValue);
                            if (severityPtr != nullptr)
                            {
                                severity = *severityPtr;
                            }
                        }
                        else if (prop == "Resolution")
                        {
                            auto calloutsPtr =
                                std::get_if<std::string>(&propValue);
                            if (calloutsPtr != nullptr)
                            {
                                callouts = *calloutsPtr;
                            }
                        }
                        else if (prop == "EventId")
                        {
                            auto eventIdPtr =
                                std::get_if<std::string>(&propValue);
                            if (eventIdPtr != nullptr)
                            {
                                // EventId B700900B 00000072 ...
                                // First value is RefCode
                                std::istringstream iss(*eventIdPtr);
                                iss >> refCode;
                            }
                        }
                    }
                }
                else if (intf == "org.open_power.Logging.PEL.Entry")
                {
                    for (const auto& [prop, propValue] : properties)
                    {
                        if (prop == "PlatformLogID")
                        {
                            auto plidPtr = std::get_if<uint32_t>(&propValue);
                            if (plidPtr != nullptr)
                            {
                                plid = *plidPtr;
                            }
                        }
                        else if (prop == "Deconfig")
                        {
                            auto deconfigPtr = std::get_if<bool>(&propValue);
                            if (deconfigPtr != nullptr)
                            {
                                deconfigured = *deconfigPtr;
                            }
                        }
                        else if (prop == "Guard")
                        {
                            auto guardPtr = std::get_if<bool>(&propValue);
                            if (guardPtr != nullptr)
                            {
                                guarded = *guardPtr;
                            }
                        }
                        else if (prop == "Timestamp")
                        {
                            auto timestampPtr =
                                std::get_if<uint64_t>(&propValue);
                            if (timestampPtr != nullptr)
                            {
                                timestamp = *timestampPtr;
                            }
                        }
                    }
                }
            }

            if (resolved == true)
            {
                continue;
            }

            // ignore informational and debug errors
            if ((severity == "xyz.openbmc_project.Logging.Entry.Level.Debug") ||
                (severity == "xyz.openbmc_project.Logging.Entry.Level."
                             "Informational") ||
                (severity == "xyz.openbmc_project.Logging.Entry.Level.Notice"))
            {
                continue;
            }

            if (deconfigured == false)
            {
                continue;
            }

            if (guarded == true)
            {
                continue; // will be captured as part of guard records
            }

            // Ignore PELS that are created before chassis poweron
            if (timestamp < poweronTimestamp)
            {
                lg2::info("Ignoring PEL created before chassis "
                          "poweron {OBJECT}",
                          "OBJECT", path.str);
                continue;
            }

            // add cec errorlog
            json jsonErrorLog = json::object();
            std::stringstream ss;
            ss << std::hex << "0x" << plid;
            jsonErrorLog["PLID"] = ss.str();
            jsonErrorLog["Callout Section"] = parseCallout(callouts);
            jsonErrorLog["SRC"] = refCode;
            jsonErrorLog["DATE_TIME"] = epochTimeToBCD(timestamp);

            json jsonErrorLogSection = json::array();
            jsonErrorLogSection.push_back(std::move(jsonErrorLog));

            // add resource action check if guard record is found
            json jsonResource = json::object();
            for (const auto& elem : guardRecords)
            {
                if (elem.elogId == plid)
                {
                    auto physicalPath =
                        openpower::guard::getPhysicalPath(elem.targetId);
                    GuardedTarget guardedTarget(*physicalPath);
                    pdbg_target_traverse(nullptr, getGuardedTarget,
                                         &guardedTarget);
                    if (guardedTarget.target == nullptr)
                    {
                        lg2::info("Failed to find the pdbg target for "
                                  "guarded "
                                  "target {RECORD_ID}",
                                  "RECORD_ID", elem.recordId);
                        continue;
                    }
                    jsonResource["TYPE"] = pdbgTargetName(guardedTarget.target);
                    std::string state = stateDeconfigured;
                    ATTR_HWAS_STATE_Type hwasState;
                    if (!DT_GET_PROP(ATTR_HWAS_STATE, guardedTarget.target,
                                     hwasState))
                    {
                        if (hwasState.functional)
                        {
                            state = stateConfigured;
                        }
                    }
                    jsonResource["CURRENT_STATE"] = std::move(state);

                    ATTR_LOCATION_CODE_Type attrLocCode;
                    if (!DT_GET_PROP(ATTR_LOCATION_CODE, guardedTarget.target,
                                     attrLocCode))
                    {
                        jsonResource["LOCATION_CODE"] = attrLocCode;
                    }

                    jsonResource["REASON_DESCRIPTION"] =
                        getGuardReason(guardRecords, *physicalPath);

                    jsonResource["GARD_RECORD"] = true;

                    break;
                }
            } // endfor
            json jsonEventData = json::object();
            jsonEventData["RESOURCE_ACTIONS"] = std::move(jsonResource);
            jsonErrorLogSection.push_back(jsonEventData);

            json jsonErrlogObj = json::object();
            jsonErrlogObj["CEC_ERROR_LOG"] = std::move(jsonErrorLogSection);
            json jsonServiceEvent = json::object();
            jsonServiceEvent["SERVICEABLE_EVENT"] = std::move(jsonErrlogObj);
            jsonNag.push_back(std::move(jsonServiceEvent));
        }
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        lg2::info("There are no PELS or PEL corresponding to guard record is "
                  "deleted "
                  " {ERROR}",
                  "ERROR", ex);
    }
    catch (const std::exception& ex)
    {
        lg2::error("Failed to add unresolved pels with deconfig bit "
                   "set {ERROR}",
                   "ERROR", ex);
    }
}
} // namespace openpower::faultlog
