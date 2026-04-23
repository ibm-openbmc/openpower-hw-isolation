#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdeventplus/event.hpp>
#include <service_alerts_time.hpp>

int main(int /*argc*/, char** /*argv*/)
{
    try
    {
        lg2::info(
            "faultlog service alerts monitor - watching property changes");
        auto bus = sdbusplus::bus::new_default();
        auto event = sdeventplus::Event::get_default();

        // Start watching for service alerts property changes
        openpower::faultlog::watchServiceAlertsProperty(bus);

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

// Made with Bob
