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

    // read timestamp from file
    const uint64_t poweronTimestamp = readPowerOnTime(bus);

    forEachPEL(bus, [&](const sdbusplus::message::object_path& path,
                        const PELProperties& props) {
        if (props.resolved)
        {
            return true; // Continue
        }

        // ignore informational and debug errors
        if ((props.severity ==
             "xyz.openbmc_project.Logging.Entry.Level.Debug") ||
            (props.severity ==
             "xyz.openbmc_project.Logging.Entry.Level.Informational") ||
            (props.severity ==
             "xyz.openbmc_project.Logging.Entry.Level.Notice"))
        {
            return true; // Continue
        }

        if (!props.deconfigured || props.hidden || props.guarded)
        {
            return true; // Continue
        }

        // power and thermal err src starts with 1100
        const bool pwrThermalErr =
            props.refCode.substr(0, pwrThermalErrPrefix.length()) ==
            pwrThermalErrPrefix;

        // during IPL ignore power and thermal
        if (ignorePwrFanPel && pwrThermalErr)
        {
            lg2::info("Ignoring power/thermal PEL as system is IPLing {OBJECT}",
                      "OBJECT", path.str);
            return true; // Continue
        }

        // Ignore power and thermal pels if poweron timestamp is not found
        if (pwrThermalErr && poweronTimestamp == 0)
        {
            lg2::info("Ignoring power/thermal PEL as poweron timestamp "
                      "is not found {OBJECT}",
                      "OBJECT", path.str);
            return true; // Continue
        }

        // Ignore PELS that are created before chassis poweron
        if (props.timestamp < poweronTimestamp)
        {
            return true; // Continue
        }

        count += 1;
        return true; // Continue
    });

    return count;
}

void UnresolvedPELs::populate(sdbusplus::bus::bus& bus,
                              const GuardRecords& guardRecords, json& jsonNag)
{
    const uint64_t poweronTimestamp = readPowerOnTime(bus);

    forEachPEL(bus, [&](const sdbusplus::message::object_path& path,
                        const PELProperties& props) {
        if (props.resolved)
        {
            return true; // Continue
        }

        // ignore informational and debug errors
        if ((props.severity ==
             "xyz.openbmc_project.Logging.Entry.Level.Debug") ||
            (props.severity ==
             "xyz.openbmc_project.Logging.Entry.Level.Informational") ||
            (props.severity ==
             "xyz.openbmc_project.Logging.Entry.Level.Notice"))
        {
            return true; // Continue
        }

        if (!props.deconfigured || props.guarded || props.hidden)
        {
            return true; // Continue
        }

        // power and thermal err src starts with 1100
        const bool pwrThermalErr =
            props.refCode.substr(0, pwrThermalErrPrefix.length()) ==
            pwrThermalErrPrefix;

        // Ignore power and thermal pels if poweron timestamp is not found
        if (pwrThermalErr && poweronTimestamp == 0)
        {
            lg2::debug("Ignoring power/thermal PEL as poweron timestamp "
                       "is not found {OBJECT}",
                       "OBJECT", path.str);
            return true; // Continue
        }

        // Ignore PELS that are created before chassis poweron
        if (props.timestamp < poweronTimestamp)
        {
            lg2::debug("Ignoring PEL created before chassis poweron {OBJECT}",
                       "OBJECT", path.str);
            return true; // Continue
        }

        // add cec errorlog
        json jsonErrorLog = json::object();
        std::stringstream ss;
        ss << std::hex << "0x" << props.plid;
        jsonErrorLog["PLID"] = ss.str();
        jsonErrorLog["Callout Section"] = parseCallout(props.callouts);
        jsonErrorLog["SRC"] = props.refCode;
        jsonErrorLog["DATE_TIME"] = epochTimeToBCD(props.timestamp);

        json jsonErrorLogSection = json::array();
        jsonErrorLogSection.push_back(std::move(jsonErrorLog));

        // add resource action check if guard record is found
        json jsonResource = json::object();
        for (const auto& elem : guardRecords)
        {
            if (elem.elogId == props.plid)
            {
                auto physicalPath =
                    openpower::guard::getPhysicalPath(elem.targetId);
                GuardedTarget guardedTarget(*physicalPath);
                pdbg_target_traverse(nullptr, getGuardedTarget, &guardedTarget);
                if (guardedTarget.target == nullptr)
                {
                    lg2::info("Failed to find the pdbg target for guarded "
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
                // gets it from parent target
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
        }
        json jsonEventData = json::object();
        jsonEventData["RESOURCE_ACTIONS"] = std::move(jsonResource);
        jsonErrorLogSection.push_back(jsonEventData);

        json jsonErrlogObj = json::object();
        jsonErrlogObj["CEC_ERROR_LOG"] = std::move(jsonErrorLogSection);
        jsonNag.emplace_back(jsonErrlogObj);

        return true; // Continue
    });
}
} // namespace openpower::faultlog
