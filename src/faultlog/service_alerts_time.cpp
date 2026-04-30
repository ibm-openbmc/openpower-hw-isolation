#include <phosphor-logging/lg2.hpp>
#include <service_alerts_time.hpp>
#include <util.hpp>

#include <filesystem>
#include <fstream>

namespace openpower::faultlog
{

// Service alerts control constants
constexpr auto SEND_SERVICE_ALERTS_PATH =
    "/xyz/openbmc_project/hardware_isolation/send_service_alerts";
constexpr auto SEND_SERVICE_ALERTS_IFACE = "xyz.openbmc_project.Object.Enable";
constexpr auto NAG_DISABLED_TIMESTAMP_FILE =
    "/var/lib/phosphor-faultlog/nag_disabled_time";

bool isSendServiceAlertsEnabled(sdbusplus::bus::bus& bus)
{
    try
    {
        bool enabled = readProperty<bool>(bus, "xyz.openbmc_project.Settings",
                                          SEND_SERVICE_ALERTS_PATH,
                                          SEND_SERVICE_ALERTS_IFACE, "Enabled");
        lg2::info("Service alerts status: {STATUS}", "STATUS",
                  enabled ? "enabled" : "disabled");
        return enabled;
    }
    catch (const std::exception& ex)
    {
        lg2::info("Failed to read service alerts property, defaulting to "
                  "enabled: {ERROR}",
                  "ERROR", ex);
        return true; // Default to enabled on error
    }
}

void writeDisabledTimestamp(const std::string& filePath)
{
    const auto targetFile = filePath.empty() ? NAG_DISABLED_TIMESTAMP_FILE
                                             : filePath;

    try
    {
        // Ensure directory exists
        const std::filesystem::path path(targetFile);
        std::filesystem::create_directories(path.parent_path());

        std::ofstream file(targetFile);
        if (!file)
        {
            lg2::error("Failed to open timestamp file for writing");
            return;
        }

        const auto now = std::time(nullptr);
        file << now;
        lg2::info("Wrote nag disabled timestamp: {TIMESTAMP}", "TIMESTAMP",
                  now);
    }
    catch (const std::exception& ex)
    {
        lg2::error("Failed to write disabled timestamp: {ERROR}", "ERROR", ex);
    }
}

std::optional<time_t> readDisabledTimestamp(const std::string& filePath)
{
    const auto targetFile = filePath.empty() ? NAG_DISABLED_TIMESTAMP_FILE
                                             : filePath;

    try
    {
        if (!std::filesystem::exists(targetFile))
        {
            return std::nullopt;
        }

        std::ifstream file(targetFile);
        if (!file)
        {
            return std::nullopt;
        }

        time_t timestamp{};
        file >> timestamp;
        lg2::info("Read nag disabled timestamp: {TIMESTAMP}", "TIMESTAMP",
                  timestamp);
        return timestamp;
    }
    catch (const std::exception& ex)
    {
        lg2::error("Failed to read disabled timestamp: {ERROR}", "ERROR", ex);
        return std::nullopt;
    }
}

} // namespace openpower::faultlog
