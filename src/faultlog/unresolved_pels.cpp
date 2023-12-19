#include <attributes_info.H>
#include <libphal.H>

#include <libguard/guard_interface.hpp>
#include <phosphor-logging/log.hpp>
#include <poweron_time.hpp>
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
    GuardedTarget(const std::string& path) : phyDevPath(path) {}
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

        // read timestamp from file
        uint64_t poweronTimestamp = readPowerOnTime(bus);

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

            // power and thermal err src starts with 1100
            bool pwrThermalErr = false;
            if (refCode.substr(0, pwrThermalErrPrefix.length()) ==
                pwrThermalErrPrefix)
            {
                pwrThermalErr = true;
            }

            // during IPL ignore power and thermal errors
            if (ignorePwrFanPel && pwrThermalErr)
            {
                lg2::info("Ignoring power/thermal PEL as system is IPLing "
                          "{OBJECT}",
                          "OBJECT", path.str);
                continue;
            }

            // Ignore power and thermal pels if poweron timestamp is not found
            if (pwrThermalErr && poweronTimestamp == 0)
            {
                lg2::info("Ignoring power/thermal PEL as poweron timestamp "
                          "is not found {OBJECT}",
                          "OBJECT", path.str);
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

        uint64_t poweronTimestamp = readPowerOnTime(bus);

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

            // power and thermal err src starts with 1100
            bool pwrThermalErr = false;
            if (refCode.substr(0, pwrThermalErrPrefix.length()) ==
                pwrThermalErrPrefix)
            {
                pwrThermalErr = true;
            }

            // Ignore power and thermal pels if poweron timestamp is not found
            if (pwrThermalErr && poweronTimestamp == 0)
            {
                lg2::debug("Ignoring power/thermal PEL as poweron timestamp "
                           "is not found {OBJECT}",
                           "OBJECT", path.str);
                continue;
            }

            // Ignore PELS that are created before chassis poweron
            if (timestamp < poweronTimestamp)
            {
                lg2::debug("Ignoring PEL created before chassis "
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

                    // getLocationCode checks if attr is present in target else
                    // gets it from partent target
                    ATTR_LOCATION_CODE_Type attrLocCode = {'\0'};
                    openpower::phal::pdbg::getLocationCode(guardedTarget.target,
                                                           attrLocCode);
                    jsonResource["LOCATION_CODE"] = attrLocCode;

                    jsonResource["REASON_DESCRIPTION"] =
                        getGuardReason(guardRecords, *physicalPath);

                    jsonResource["GUARD_RECORD"] = true;
                    ATTR_PHYS_DEV_PATH_Type phyPath;
                    if (!DT_GET_PROP(ATTR_PHYS_DEV_PATH, guardedTarget.target,
                                     phyPath))
                    {
                        jsonResource["PHYS_PATH"] = phyPath;
                    }

                    break;
                }
            } // endfor
            json jsonEventData = json::object();
            jsonEventData["RESOURCE_ACTIONS"] = std::move(jsonResource);
            jsonErrorLogSection.push_back(jsonEventData);

            json jsonErrlogObj = json::object();
            jsonErrlogObj["CEC_ERROR_LOG"] = std::move(jsonErrorLogSection);
            jsonNag.emplace_back(jsonErrlogObj);
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
