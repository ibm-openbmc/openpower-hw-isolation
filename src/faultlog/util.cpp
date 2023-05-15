#include <libguard/guard_interface.hpp>
#include <sdbusplus/exception.hpp>
#include <util.hpp>

#include <iomanip>
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
        std::string temp(*physicalPath);
        if (temp.find(path) != std::string::npos)
        {
            return openpower::guard::guardReasonToStr(elem.errType);
        }
    }
    return "Unknown";
}
ProgressStages getBootProgress(sdbusplus::bus::bus& bus)
{
    try
    {
        using PropertiesVariant =
            sdbusplus::utility::dedup_variant_t<ProgressStages>;

        auto retVal = readProperty<PropertiesVariant>(
            bus, "xyz.openbmc_project.State.Host",
            "/xyz/openbmc_project/state/host0",
            "xyz.openbmc_project.State.Boot.Progress", "BootProgress");
        const ProgressStages* progPtr = std::get_if<ProgressStages>(&retVal);
        if (progPtr != nullptr)
        {
            return *progPtr;
        }
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
        using PropertiesVariant =
            sdbusplus::utility::dedup_variant_t<HostState>;

        auto retVal = readProperty<PropertiesVariant>(
            bus, "xyz.openbmc_project.State.Host",
            "/xyz/openbmc_project/state/host0",
            "xyz.openbmc_project.State.Host", "CurrentHostState");
        const HostState* statePtr = std::get_if<HostState>(&retVal);
        if (statePtr != nullptr)
        {
            return *statePtr;
        }
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

std::string epochTimeToBCD(uint64_t milliSeconds)
{
    auto decimalToBCD = [](int value) {
        std::stringstream ss;
        ss << std::setw(2) << std::setfill('0') << value;
        return ss.str();
    };
    std::chrono::milliseconds ms{milliSeconds};
    std::chrono::time_point<std::chrono::system_clock> time{ms};

    time_t t = std::chrono::system_clock::to_time_t(time);
    tm* timeInfo = localtime(&t);

    int year = 1900 + timeInfo->tm_year;
    std::string bcdYear = decimalToBCD(year / 100);
    bcdYear += decimalToBCD(year % 100);
    std::string bcdMonth = decimalToBCD(timeInfo->tm_mon + 1); // Month (1-12)
    std::string bcdDay = decimalToBCD(timeInfo->tm_mday);  // Day of the month
    std::string bcdHour = decimalToBCD(timeInfo->tm_hour); // Hour (0-23)
    std::string bcdMin = decimalToBCD(timeInfo->tm_min);   // Minutes
    std::string bcdSec = decimalToBCD(timeInfo->tm_sec);   // Seconds

    // Construct the BCD time string
    //"DATE_TIME": "04/11/2023 09:39:15",
    std::string bcdTime = bcdMonth + "/" + bcdDay + "/" + bcdYear + " " +
                          bcdHour + ":" + bcdMin + ":" + bcdSec;
    return bcdTime;
}

json parseCallout(const std::string callout)
{
    json callouthdrJson = json::object();
    if (callout.empty())
    {
        return callouthdrJson;
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

} // namespace openpower::faultlog
