#include <attributes_info.H>

#include <libguard/guard_interface.hpp>
#include <sdbusplus/exception.hpp>
#include <util.hpp>

#include <regex>
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
    std::istringstream stream(callout);
    std::string line;

    // Regular expression to capture key-value pairs (ignores the starting
    // number)
    // Example
    // 1. LocationCode:xxxx, CCIN:XXX, SN:xxxx, PN:xxxx, Priority:xxx
    // 2. PN:xxxx, Priority:xxx
    std::regex pattern(
        R"((Location Code|Priority|PN|SN|CCIN):\s*([A-Za-z0-9.-]+))");

    int lineCount = 0;
    json calloutsJson = json::array();
    // Read each line and parse it directly into a JSON object
    while (std::getline(stream, line))
    {
        if (!line.empty())
        {                    // Ignore empty lines
            lineCount += 1;
            json jsonObject; // JSON object to hold key-value pairs
            std::smatch matches;
            std::string::const_iterator searchStart(line.cbegin());

            // Use regex to find all key-value pairs in the current line
            while (
                std::regex_search(searchStart, line.cend(), matches, pattern))
            {
                std::string key = matches[1].str();
                if (key == "SN")
                {
                    key = "Serial Number";
                }
                else if (key == "PN")
                {
                    key = "Part Number";
                }
                jsonObject[key] =
                    matches[2].str(); // Assign key-value pairs to JSON object
                searchStart = matches.suffix().first; // Move to the next match
            }
            calloutsJson.push_back(
                jsonObject); // Add the JSON object to the array
        }
    }
    json sectionJson = json::object();
    sectionJson["Callout Count"] = lineCount;
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
