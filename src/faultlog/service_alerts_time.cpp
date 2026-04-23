#include <phosphor-logging/lg2.hpp>
#include <service_alerts_time.hpp>
#include <util.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ranges>

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

bool hasNewErrorsSinceDisabled(sdbusplus::bus::bus& bus,
                               const GuardRecords& unresolvedRecords,
                               time_t disabledTime)
{
    const auto disabledTimeU64 = static_cast<uint64_t>(disabledTime);

    // Check guard records by looking up their associated PEL timestamps
    for (const auto& record : unresolvedRecords)
    {
        // Skip manual guard records (no associated PEL)
        if (record.elogId == 0)
        {
            continue;
        }

        try
        {
            // Get BMC log ID from PEL ID
            auto method = bus.new_method_call(
                "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
                "org.open_power.Logging.PEL", "GetBMCLogIdFromPELId");
            method.append(static_cast<uint32_t>(record.elogId));
            auto resp = bus.call(method);
            uint32_t bmcLogId = 0;
            resp.read(bmcLogId);

            // Get PEL timestamp
            const std::string objPath = "/xyz/openbmc_project/logging/entry/" +
                                        std::to_string(bmcLogId);
            auto pelMethod = bus.new_method_call(
                "xyz.openbmc_project.Logging", objPath.c_str(),
                "org.freedesktop.DBus.Properties", "Get");
            pelMethod.append("org.open_power.Logging.PEL.Entry", "Timestamp");
            auto pelReply = bus.call(pelMethod);
            std::variant<uint64_t> timestampVariant;
            pelReply.read(timestampVariant);
            const auto timestamp = std::get<uint64_t>(timestampVariant);

            // Convert milliseconds to seconds
            constexpr uint64_t msToSeconds = 1000;
            const auto timestampSeconds = timestamp / msToSeconds;

            if (timestampSeconds > disabledTimeU64)
            {
                lg2::info("Found new guard record after nagging was disabled. "
                          "Record ID: {RECORD_ID}, PEL ID: {ELOG_ID}, "
                          "Timestamp: {TIMESTAMP}",
                          "RECORD_ID", record.recordId, "ELOG_ID",
                          record.elogId, "TIMESTAMP", timestampSeconds);
                return true;
            }
        }
        catch (const std::exception& ex)
        {
            // PEL might be deleted, skip this record
            lg2::info("Could not get timestamp for guard record {RECORD_ID}, "
                      "PEL ID {ELOG_ID}: {ERROR}",
                      "RECORD_ID", record.recordId, "ELOG_ID", record.elogId,
                      "ERROR", ex);
        }
    }

    // Check unresolved PELs with deconfig bit set (not yet guarded)
    return checkUnresolvedPELs(bus, disabledTimeU64);
}

bool checkUnresolvedPELs(sdbusplus::bus::bus& bus, uint64_t disabledTime)
{
    bool foundNewPEL = false;

    forEachPEL(bus, [&](const sdbusplus::message::object_path& path,
                        const PELProperties& props) {
        // Check if this is an unresolved, serviceable PEL
        // Skip guarded PELs as they are captured as guard records
        if (!props.resolved && props.deconfigured && !props.guarded &&
            !props.hidden)
        {
            // Convert milliseconds to seconds and check if newer than disabled
            // time
            constexpr uint64_t msToSeconds = 1000;
            const auto pelTimestampSeconds = props.timestamp / msToSeconds;
            if (pelTimestampSeconds > disabledTime)
            {
                lg2::info("Found new unresolved PEL after nagging was "
                          "disabled. Path: {PATH}, Timestamp: {TIMESTAMP}",
                          "PATH", path.str, "TIMESTAMP", pelTimestampSeconds);
                foundNewPEL = true;
                return false; // Stop iteration
            }
        }
        return true; // Continue iteration
    });

    return foundNewPEL;
}

void enableServiceAlerts(sdbusplus::bus::bus& bus)
{
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
        }
        else
        {
            lg2::info("Service alerts re-enabled due to new errors");
            // Remove timestamp file
            if (std::filesystem::exists(NAG_DISABLED_TIMESTAMP_FILE))
            {
                std::filesystem::remove(NAG_DISABLED_TIMESTAMP_FILE);
            }
        }
    }
    catch (const std::exception& ex)
    {
        lg2::error("Failed to enable service alerts: {ERROR}", "ERROR", ex);
    }
}

void watchServiceAlertsProperty(sdbusplus::bus::bus& bus)
{
    static std::unique_ptr<sdbusplus::bus::match_t> match;

    match = std::make_unique<sdbusplus::bus::match_t>(
        bus,
        sdbusplus::bus::match::rules::propertiesChanged(
            SEND_SERVICE_ALERTS_PATH, SEND_SERVICE_ALERTS_IFACE),
        [](sdbusplus::message::message& msg) {
        std::string intf;
        std::map<std::string, std::variant<bool>> properties;
        msg.read(intf, properties);

        const auto it = properties.find("Enabled");
        if (it == properties.end())
        {
            return;
        }

        const bool enabled = std::get<bool>(it->second);
        lg2::info("Service alerts property changed to: {STATUS}", "STATUS",
                  enabled ? "enabled" : "disabled");

        if (!enabled)
        {
            // ALWAYS write/update timestamp when disabled
            writeDisabledTimestamp();
            lg2::info("Updated nag disabled timestamp due to property change");
        }
        else
        {
            // When re-enabled, remove the timestamp file
            try
            {
                if (std::filesystem::exists(NAG_DISABLED_TIMESTAMP_FILE))
                {
                    std::filesystem::remove(NAG_DISABLED_TIMESTAMP_FILE);
                    lg2::info(
                        "Removed nag disabled timestamp file - alerts enabled");
                }
            }
            catch (const std::exception& ex)
            {
                lg2::error("Failed to remove timestamp file: {ERROR}", "ERROR",
                           ex);
            }
        }
    });
}

} // namespace openpower::faultlog

// Made with Bob
