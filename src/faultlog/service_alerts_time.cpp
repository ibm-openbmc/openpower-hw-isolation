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

bool checkUnresolvedPELs(sdbusplus::bus::bus& bus, uint64_t disabledTime)
{
    bool foundNewPEL = false;

    try
    {
        using PropertyValue =
            std::variant<std::string, bool, uint8_t, int16_t, uint16_t, int32_t,
                         uint32_t, int64_t, uint64_t, double>;
        using Properties = std::map<std::string, PropertyValue>;
        using Interfaces = std::map<std::string, Properties>;
        using Objects = std::map<sdbusplus::message::object_path, Interfaces>;

        Objects objects;
        auto method = bus.new_method_call(
            "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
        auto reply = bus.call(method);
        reply.read(objects);

        for (const auto& [path, interfaces] : objects)
        {
            bool resolved = true;
            bool deconfigured = false;
            bool hidden = false;
            bool guarded = false;
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
                        else if (prop == "Hidden")
                        {
                            auto hiddenPtr = std::get_if<bool>(&propValue);
                            if (hiddenPtr != nullptr)
                            {
                                hidden = *hiddenPtr;
                            }
                        }
                    }
                }
            }

            // Check if this is an unresolved, serviceable PEL
            // pel time stamp
            if (!resolved && (deconfigured || guarded) && !hidden)
            {
                // Convert milliseconds to seconds and check if newer than
                // disabled time
                constexpr uint64_t msToSeconds = 1000;
                const auto pelTimestampSeconds = timestamp / msToSeconds;
                if (pelTimestampSeconds > disabledTime)
                {
                    lg2::info("Found new unresolved PEL after nagging was "
                              "disabled. Path: {PATH}, Timestamp: {TIMESTAMP}",
                              "PATH", path.str, "TIMESTAMP",
                              pelTimestampSeconds);
                    foundNewPEL = true;
                    break; // Stop iteration
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to check unresolved PELs: {ERROR}", "ERROR", e);
    }

    return foundNewPEL;
}

void enableServiceAlerts(sdbusplus::bus::bus& bus)
{
    using namespace openpower::faultlog;
    try
    {
        auto method = bus.new_method_call(
            "xyz.openbmc_project.Settings", SEND_SERVICE_ALERTS_PATH,
            "org.freedesktop.DBus.Properties", "Set");
        method.append(SEND_SERVICE_ALERTS_IFACE, "Enabled",
                      std::variant<bool>(true));
        auto reply = bus.call(method);
        if (reply.is_method_error())
        {
            lg2::error("Failed to re-enable service alerts via D-Bus");
            return;
        }
        lg2::info("Service alerts re-enabled due to new errors");
        // Remove timestamp file
        if (std::filesystem::exists(NAG_DISABLED_TIMESTAMP_FILE))
        {
            std::filesystem::remove(NAG_DISABLED_TIMESTAMP_FILE);
        }
    }
    catch (const std::exception& ex)
    {
        lg2::error("Failed to enable service alerts: {ERROR}", "ERROR", ex);
    }
}
} // namespace openpower::faultlog
