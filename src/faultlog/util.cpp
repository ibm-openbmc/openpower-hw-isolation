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

bool forEachPEL(sdbusplus::bus::bus& bus,
                std::function<bool(const sdbusplus::message::object_path&,
                                   const PELProperties&)>
                    callback)
{
    try
    {
        using PropertyValue =
            std::variant<std::string, bool, uint8_t, int16_t, uint16_t, int32_t,
                         uint32_t, int64_t, uint64_t, double>;
        using Properties = std::map<std::string, PropertyValue>;
        using Interfaces = std::map<std::string, Properties>;
        using Objects = std::map<sdbusplus::message::object_path, Interfaces>;

        auto method = bus.new_method_call(
            "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
        auto reply = bus.call(method);

        Objects objects;
        reply.read(objects);

        for (const auto& [path, interfaces] : objects)
        {
            PELProperties props;

            // Parse logging entry interface
            for (const auto& [intf, properties] : interfaces)
            {
                if (intf == "xyz.openbmc_project.Logging.Entry")
                {
                    for (const auto& [prop, propValue] : properties)
                    {
                        if (prop == "Resolved")
                        {
                            if (const auto* ptr = std::get_if<bool>(&propValue))
                            {
                                props.resolved = *ptr;
                            }
                        }
                        else if (prop == "Severity")
                        {
                            if (const auto* ptr =
                                    std::get_if<std::string>(&propValue))
                            {
                                props.severity = *ptr;
                            }
                        }
                        else if (prop == "EventId")
                        {
                            if (const auto* ptr =
                                    std::get_if<std::string>(&propValue))
                            {
                                // EventId B700900B 00000072 00010016 ...
                                // First value is RefCode
                                std::istringstream iss(*ptr);
                                iss >> props.refCode;
                            }
                        }
                        else if (prop == "Resolution")
                        {
                            if (const auto* ptr =
                                    std::get_if<std::string>(&propValue))
                            {
                                props.callouts = *ptr;
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
                            if (const auto* ptr =
                                    std::get_if<uint32_t>(&propValue))
                            {
                                props.plid = *ptr;
                            }
                        }
                        else if (prop == "Deconfig")
                        {
                            if (const auto* ptr = std::get_if<bool>(&propValue))
                            {
                                props.deconfigured = *ptr;
                            }
                        }
                        else if (prop == "Guard")
                        {
                            if (const auto* ptr = std::get_if<bool>(&propValue))
                            {
                                props.guarded = *ptr;
                            }
                        }
                        else if (prop == "Hidden")
                        {
                            if (const auto* ptr = std::get_if<bool>(&propValue))
                            {
                                props.hidden = *ptr;
                            }
                        }
                        else if (prop == "Timestamp")
                        {
                            if (const auto* ptr =
                                    std::get_if<uint64_t>(&propValue))
                            {
                                props.timestamp = *ptr;
                            }
                        }
                    }
                }
            }

            // Invoke callback - if it returns false, stop iteration
            if (!callback(path, props))
            {
                return true;
            }
        }
        return true;
    }
    catch (const std::exception& ex)
    {
        lg2::error("Failed to iterate PELs: {ERROR}", "ERROR", ex);
        return false;
    }
}

} // namespace openpower::faultlog
