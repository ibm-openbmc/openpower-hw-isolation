#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdeventplus/event.hpp>
#include <service_alerts_time.hpp>

#include <filesystem>
#include <memory>

namespace
{
constexpr auto SEND_SERVICE_ALERTS_PATH =
    "/xyz/openbmc_project/hardware_isolation/send_service_alerts";
constexpr auto SEND_SERVICE_ALERTS_IFACE = "xyz.openbmc_project.Object.Enable";
constexpr auto NAG_DISABLED_TIMESTAMP_FILE =
    "/var/lib/phosphor-faultlog/nag_disabled_time";
} // namespace

int main(int /*argc*/, char** /*argv*/)
{
    try
    {
        lg2::info(
            "faultlog service alerts monitor - watching property changes");
        auto bus = sdbusplus::bus::new_default();
        auto event = sdeventplus::Event::get_default();

        // Watch for service alerts property changes
        static std::unique_ptr<sdbusplus::bus::match_t> match;
        match = std::make_unique<sdbusplus::bus::match_t>(
            bus,
            sdbusplus::bus::match::rules::propertiesChanged(
                SEND_SERVICE_ALERTS_PATH, SEND_SERVICE_ALERTS_IFACE),
            [](sdbusplus::message_t& msg) {
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
                openpower::faultlog::writeDisabledTimestamp();
                lg2::info(
                    "Updated nag disabled timestamp due to property change");
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
                    lg2::error("Failed to remove timestamp file: {ERROR}",
                               "ERROR", ex);
                }
            }
        });

        // Attach bus to event loop
        bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);

        // Run the event loop
        return event.loop();
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed {ERROR}", "ERROR", e.what());
        return 1;
    }
}
