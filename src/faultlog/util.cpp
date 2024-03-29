#include <attributes_info.H>

#include <libguard/guard_interface.hpp>
#include <sdbusplus/exception.hpp>
#include <util.hpp>

#include <sstream>

namespace openpower::faultlog
{

using ProgressStages = sdbusplus::xyz::openbmc_project::State::Boot::server::
    Progress::ProgressStages;
using HostState =
    sdbusplus::xyz::openbmc_project::State::server::Host::HostState;

std::string getGuardReason(const GuardRecords& guardRecords,
                           const std::string& path)
{
    for (const auto& elem : guardRecords)
    {
        auto physicalPath = openpower::guard::getPhysicalPath(elem.targetId);
        if (!physicalPath.has_value())
        {
            lg2::error("Failed to get physical path for record {RECORD_ID}",
                       "RECORD_ID", elem.recordId);
            continue;
        }
        std::string phyPath(*physicalPath);
        if (phyPath.find(path) != std::string::npos)
        {
            std::string reason =
                openpower::guard::guardReasonToStr(elem.errType);
            std::transform(reason.begin(), reason.end(), reason.begin(),
                           ::toupper);
            return reason;
        }
    }
    return "UNKNOWN";
}
ProgressStages getBootProgress(sdbusplus::bus::bus& bus)
{
    try
    {
        return readProperty<ProgressStages>(
            bus, "xyz.openbmc_project.State.Host",
            "/xyz/openbmc_project/state/host0",
            "xyz.openbmc_project.State.Boot.Progress", "BootProgress");
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        lg2::error("Failed to read Boot Progress property {ERROR}", "ERROR",
                   ex.what());
    }

    lg2::error("Failed to read Boot Progress state value");
    return ProgressStages::Unspecified;
}

HostState getHostState(sdbusplus::bus::bus& bus)
{
    try
    {
        return readProperty<HostState>(bus, "xyz.openbmc_project.State.Host",
                                       "/xyz/openbmc_project/state/host0",
                                       "xyz.openbmc_project.State.Host",
                                       "CurrentHostState");
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        lg2::error("Failed to read host state property {ERROR}", "ERROR",
                   ex.what());
    }

    lg2::error("Failed to read host state value");
    return HostState::Off;
}

bool isHostProgressStateRunning(sdbusplus::bus::bus& bus)
{
    ProgressStages progress = getBootProgress(bus);
    if ((progress == ProgressStages::SystemInitComplete) ||
        (progress == ProgressStages::SystemSetup) ||
        (progress == ProgressStages::OSStart) ||
        (progress == ProgressStages::OSRunning))
    {
        return true;
    }
    return false;
}

bool isHostStateRunning(sdbusplus::bus::bus& bus)
{
    return getHostState(bus) == HostState::Running;
}

json parseCallout(const std::string callout)
{
    if (callout.empty())
    {
        return json::object();
    }

    // lambda method to split the string based on delimiter
    auto splitString = [](const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;

        while (std::getline(ss, token, delimiter))
        {
            tokens.push_back(token);
        }

        return tokens;
    };

    std::vector<std::string> lines = splitString(callout, '\n');
    json calloutsJson = json::array();

    for (const auto& line : lines)
    {
        std::vector<std::string> tokens = splitString(line, ',');
        json calloutJson = json::object();

        for (const auto& token : tokens)
        {
            std::size_t colonPos = token.find(':');

            if (colonPos != std::string::npos)
            {
                std::string key = token.substr(0, colonPos);
                key.erase(0, key.find_first_not_of(' '));
                key.erase(key.find_last_not_of(' ') + 1);
                std::string value = token.substr(colonPos + 1);
                value.erase(0, value.find_first_not_of(' '));
                value.erase(value.find_last_not_of(' ') + 1);
                if (key.find("Location Code") != std::string::npos)
                {
                    key = "Location Code";
                }
                else if (key.find("SN") != std::string::npos)
                {
                    key = "Serial Number";
                }
                else if (key.find("PN") != std::string::npos)
                {
                    key = "Part Number";
                }
                calloutJson[key] = value;
            }
        }
        calloutsJson.push_back(calloutJson);
    }
    json sectionJson = json::object();
    sectionJson["Callout Count"] = lines.size();
    sectionJson["Callouts"] = calloutsJson;
    return sectionJson;
}

bool isECOModeEnabled(struct pdbg_target* coreTgt)
{
    ATTR_ECO_MODE_Type ecoMode;
    if (DT_GET_PROP(ATTR_ECO_MODE, coreTgt, ecoMode) ||
        (ecoMode != ENUM_ATTR_ECO_MODE_ENABLED))
    {
        return false;
    }
    return true;
}

bool isECOcore(struct pdbg_target* target)
{
    const char* tgtClass = pdbg_target_class_name(target);
    if (!tgtClass)
    {
        lg2::error("Failed to get class name for the target");
        return false;
    }
    std::string strTarget(tgtClass);
    if (strTarget != "core" && strTarget != "fc")
    {
        return false;
    }
    if (strTarget == "core")
    {
        return isECOModeEnabled(target);
    }
    struct pdbg_target* coreTgt;
    pdbg_for_each_target("core", target, coreTgt)
    {
        if (isECOModeEnabled(coreTgt))
        {
            return true;
        }
    }
    return false;
}

std::string pdbgTargetName(struct pdbg_target* target)
{
    if (isECOcore(target))
    {
        return "Cache-Only Core";
    }
    auto trgtName = pdbg_target_name(target);
    return (trgtName ? trgtName : "");
}

} // namespace openpower::faultlog
