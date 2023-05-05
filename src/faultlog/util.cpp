#include <libguard/guard_interface.hpp>
#include <util.hpp>

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
    catch (const std::exception& ex)
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
        const HostState* progPtr = std::get_if<HostState>(&retVal);
        if (progPtr != nullptr)
        {
            return *progPtr;
        }
    }
    catch (const std::exception& ex)
    {
        lg2::error("Failed to read host state property {ERROR}", "ERROR",
                   ex.what());
    }

    lg2::error("Failed to read host state value");
    return HostState::Off;
}

bool isHostAppliedGuardRecords(sdbusplus::bus::bus& bus)
{
    // guard would be applied by BusInit stage
    ProgressStages progress = getBootProgress(bus);
    if ((progress >= ProgressStages::MemoryInit) ||
        (progress == ProgressStages::SecondaryProcInit) ||
        (progress == ProgressStages::PCIInit) ||
        (progress == ProgressStages::SystemInitComplete) ||
        (progress == ProgressStages::SystemSetup) ||
        (progress == ProgressStages::OSStart) ||
        (progress == ProgressStages::OSRunning))
    {
        return true;
    }
    return false;
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
    HostState hostState = getHostState(bus);
    if (hostState == HostState::Running)
    {
        return true;
    }
    return false;
}

} // namespace openpower::faultlog
