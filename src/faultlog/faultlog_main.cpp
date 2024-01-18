#include "config.h"

#include "xyz/openbmc_project/Logging/Entry/server.hpp"

#include <CLI/CLI.hpp>
#include <deconfig_records.hpp>
#include <faultlog_policy.hpp>
#include <guard_with_eid_records.hpp>
#include <guard_without_eid_records.hpp>
#include <libguard/guard_interface.hpp>
#include <libguard/include/guard_record.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdeventplus/clock.hpp>
#include <sdeventplus/source/event.hpp>
#include <sdeventplus/utility/timer.hpp>
#include <unresolved_pels.hpp>
#include <util.hpp>

#include <iostream>
#include <unordered_set>
#include <vector>
extern "C"
{
#include <libpdbg.h>
}

using namespace openpower::faultlog;
using ::nlohmann::json;
using ::openpower::guard::GuardRecords;
using Timer = sdeventplus::utility::Timer<sdeventplus::ClockId::Monotonic>;

#define GUARD_RESOLVED 0xFFFFFFFF

using Severity = sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;

using Binary = std::vector<uint8_t>;

constexpr std::chrono::milliseconds hostStateCheckTimeout(5000); // 5sec
constexpr auto IGNORE_PWR_FAN_PEL = true;

/**
 * @brief To init phal library for use power system specific device tree
 *
 * @return void
 */
void initPHAL()
{
    // Set PDBG_DTB environment variable to use interested phal cec device tree
    if (setenv("PDBG_DTB", PHAL_DEVTREE, 1))
    {
        lg2::error(
            "Failed to set PDBG_DTB with errno [{ERRNO}] and errmsg [{ERRMSG}]",
            "ERRNO", errno, "ERRMSG", strerror(errno));
        throw std::runtime_error(
            "Failed to set PDBG_DTB while trying to init PHAL");
    }
    // Set log level to info
    pdbg_set_loglevel(PDBG_ERROR);

    /**
     * Passing fdt argument as NULL so, pdbg will use PDBG_DTB environment
     * variable to get interested phal cec device tree instead of default pdbg
     * device tree.
     */
    if (!pdbg_targets_init(NULL))
    {
        throw std::runtime_error("pdbg target initialization failed");
    }
}

/** @brief Helper method to create faultlog pel
 *
 *  @param[in] bus - D-Bus to attach to
 *  @param[in] guardRecords - hardware isolated records to parse
 *  @param[in] ignorePwrFanPel - true - ignore pwr/fan pels
 */
void createNagPel(sdbusplus::bus::bus& bus,
                  const GuardRecords& unresolvedRecords, bool ignorePwrFanPel)
{
    //
    // serviceable records count
    int guardCount = GuardWithEidRecords::getCount(bus, unresolvedRecords);
    int unresolvedPelsCount = UnresolvedPELs::getCount(bus, ignorePwrFanPel);

    //
    // deconfigured records count
    int manualGuardCount = GuardWithoutEidRecords::getCount(unresolvedRecords);
    int deconfigCount = DeconfigRecords::getCount(unresolvedRecords);
    lg2::info(
        "faultlog GUARD_COUNT: {GUARD_COUNT}, MAN_GUARD_COUNT: "
        "{MAN_GUARD_COUNT}, "
        "DECONFIG_REC_COUNT: {DECONFIG_REC_COUNT} , PEL_COUNT: {PEL_COUNT} ",
        "GUARD_COUNT", guardCount, "MAN_GUARD_COUNT", manualGuardCount,
        "DECONFIG_REC_COUNT", deconfigCount, "PEL_COUNT", unresolvedPelsCount);

    // create pels only for system guard and serviceable events and not for
    // manual guard or FCO
    if ((guardCount > 0) || (unresolvedPelsCount > 0))
    {
        std::unordered_map<std::string, std::string> data = {
            {"GUARD_RECORD_COUNT", std::to_string(guardCount)},
            {"PEL_WITH_DECONFIG_BIT_COUNT",
             std::to_string(unresolvedPelsCount)}};

        auto method = bus.new_method_call(
            "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
            "xyz.openbmc_project.Logging.Create", "Create");
        method.append("org.open_power.Faultlog.Error.DeconfiguredHW",
                      Severity::Warning, data);
        auto reply = method.call();
        if (reply.is_method_error())
        {
            lg2::error("Error in calling D-Bus method to create PEL");
        }
    }
    else
    {
        lg2::info("There are no pending service actions ignoring "
                  "creating fautlog pel");
    }
}

/** @brief Get unresolved guard records
 *
 *  @return guard record list
 */
GuardRecords getGuardRecords()
{
    // Don't get ephemeral records because those type records are
    // not intended to expose to the end user, just created for
    // internal purpose to use by the BMC and Hostboot.
    openpower::guard::GuardRecords records = openpower::guard::getAll(true);
    GuardRecords unresolvedRecords;
    // filter out all unused or resolved records
    for (const auto& elem : records)
    {
        if (elem.recordId != GUARD_RESOLVED)
        {
            unresolvedRecords.emplace_back(elem);
        }
    }
    return unresolvedRecords;
}

/** @brief Callback method for boot progress property change
 *
 *  @param[in] bus - D-Bus to attach to
 *  @param[in] unresolvedRecords - hardware isolated records to parse
 *  @param[in] msg - property change D-Bus message
 */
void propertyChanged(sdbusplus::bus::bus& bus, sdbusplus::message::message& msg,
                     Timer& timer)
{
    // cancel the timer as we are getting property change requests
    // timer was added only to cater for bmc reboot when host is already
    // at running state and no property changed request will be notified
    timer.setEnabled(false);
    using ProgressStages = sdbusplus::xyz::openbmc_project::State::Boot::
        server::Progress::ProgressStages;
    using PropertiesVariant =
        sdbusplus::utility::dedup_variant_t<ProgressStages>;
    using Properties = std::map<std::string, PropertiesVariant>;
    std::string intf;
    Properties propMap;
    msg.read(intf, propMap);
    for (const auto& [prop, propValue] : propMap)
    {
        if (prop == "BootProgress")
        {
            const ProgressStages* progPtr =
                std::get_if<ProgressStages>(&propValue);
            if (progPtr != nullptr)
            {
                lg2::info("faultlog - host poweron check boot progress "
                          "value is "
                          "{BOOT_PROGRESS}",
                          "BOOT_PROGRESS", *progPtr);
                if ((*progPtr == ProgressStages::SystemInitComplete) ||
                    (*progPtr == ProgressStages::SystemSetup) ||
                    (*progPtr == ProgressStages::OSStart) ||
                    (*progPtr == ProgressStages::OSRunning))
                {
                    lg2::info("faultlog - host poweron host reached "
                              "apply guard state creating nag pel");
                    GuardRecords unresolvedRecords = getGuardRecords();
                    // ipl/poweron ignore fan/power err
                    createNagPel(bus, unresolvedRecords, IGNORE_PWR_FAN_PEL);
                    exit(EXIT_SUCCESS);
                }
            }
            else
            {
                lg2::error("Invalid property value while reading boot "
                           "progress");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        lg2::info("faultlog app to collect deconfig/guard records details");

        CLI::App app{"Faultlog tool"};
        app.set_help_flag("-h, --help", "Faultlog tool options");

        auto bus = sdbusplus::bus::new_default();
        auto event = sdeventplus::Event::get_default();
        nlohmann::json faultLogJson = json::array();

        std::string propVal{};
        try
        {
            auto pVal = readProperty<Binary>(
                bus, "xyz.openbmc_project.Inventory.Manager",
                "/xyz/openbmc_project/inventory/system/chassis/"
                "motherboard",
                "com.ibm.ipzvpd.VSYS", "TM");
            propVal.assign(reinterpret_cast<const char*>(pVal.data()),
                           pVal.size());
        }
        catch (const std::exception& ex)
        {
            std::cout << "failed to get system type " << std::endl;
        }
        nlohmann::json system;
        system["SYSTEM_TYPE"] = propVal;

        nlohmann::json systemHdr;
        systemHdr["SYSTEM"] = std::move(system);
        faultLogJson.push_back(systemHdr);

        bool guardWithEid = false;
        bool guardWithoutEid = false;
        bool policy = false;
        bool unresolvedPels = false;
        bool deconfig = false;
        bool createPel = false;
        bool listFaultlog = false;
        bool bmcReboot = false;
        bool hostPowerOn = false;

        app.set_help_flag("-h, --help", "Faultlog tool options");
        app.add_flag("-g, --guardwterr", guardWithEid,
                     "Populate guard records with associated error objects "
                     "details to JSON");
        app.add_flag("-m, --guardmanual", guardWithoutEid,
                     "Populate guard records without associated error objects "
                     "details to JSON");
        app.add_flag("-l, --policy", policy,
                     "Populate faultlog policy and FCO values to JSON");
        app.add_flag("-u, --unresolvedPels", unresolvedPels,
                     "Populate unresolved pels with deconfig bit set "
                     "details to JSON");
        app.add_flag("-d, --deconfig", deconfig,
                     "Populate deconfigured target details to JSON");
        app.add_flag("-c, --createPel", createPel,
                     "Create faultlog pel if there are guarded/deconfigured "
                     "records present");
        app.add_flag("-r, --reboot", bmcReboot,
                     "Create faultlog pel during reboot if there are "
                     "guarded/deconfigured "
                     "records present");
        app.add_flag("-p, --hostpoweron", hostPowerOn,
                     "Create faultlog pel during host power-on if there are "
                     "guarded/deconfigured "
                     "records present");
        app.add_flag("-f, --faultlog", listFaultlog,
                     "List all fault log records in JSON format");

        CLI11_PARSE(app, argc, argv);

        // create bmc reboot pel
        if (bmcReboot)
        {
            // interested only in bmc reboot, host should have been in
            // IPL runtime during bmc reboot
            if (!isHostProgressStateRunning(bus)) // host in ipl runtime
            {
                lg2::info("Ignore, host is not in running state not "
                          "bmc reboot");
                exit(EXIT_SUCCESS);
            }
        }

        initPHAL();
        openpower::guard::libguard_init(false);
        GuardRecords unresolvedRecords = getGuardRecords();

        // host will be already on, create nagpel
        if (bmcReboot)
        {
            createNagPel(bus, unresolvedRecords, !IGNORE_PWR_FAN_PEL);
        }
        // guard records with associated error object
        else if (guardWithEid)
        {
            // serviceable event records
            nlohmann::json errorlog = json::array();
            (void)GuardWithEidRecords::populate(bus, unresolvedRecords,
                                                errorlog);
            nlohmann::json jsonServiceEvent;
            jsonServiceEvent["SERVICEABLE_EVENT"] = std::move(errorlog);
            faultLogJson.emplace_back(jsonServiceEvent);
        }

        // guard records without any associated error object
        else if (guardWithoutEid)
        {
            (void)GuardWithoutEidRecords::populate(unresolvedRecords,
                                                   faultLogJson);
        }

        // guard policy
        else if (policy)
        {
            (void)FaultLogPolicy::populate(bus, faultLogJson);
        }

        // unresolved pels with deconfig bit set
        else if (unresolvedPels)
        {
            // serviceable event records
            nlohmann::json errorlog = json::array();
            (void)UnresolvedPELs::populate(bus, unresolvedRecords, errorlog);
            nlohmann::json jsonServiceEvent;
            jsonServiceEvent["SERVICEABLE_EVENT"] = std::move(errorlog);
            faultLogJson.emplace_back(jsonServiceEvent);
        }

        // pdbg targets with deconfig bit set
        else if (deconfig)
        {
            (void)DeconfigRecords::populate(unresolvedRecords, faultLogJson);
        }

        // create fault log pel if there are service actions pending
        else if (createPel)
        {
            createNagPel(bus, unresolvedRecords, !IGNORE_PWR_FAN_PEL);
        }
        // host poweron service is called both for bmc reboot and host poweron
        // need to decide bmc reboot based on host boot progress state
        else if (hostPowerOn)
        {
            if (isHostProgressStateRunning(bus))
            {
                lg2::info("faultlog hostpoweron host is already in running "
                          "state consider it as bmc reboot ");
                createNagPel(bus, unresolvedRecords,
                             !IGNORE_PWR_FAN_PEL); // add as it is bmcreboot
            }
            else
            {
                // during bmc reboot when host is already at runtime state
                // manager does not notify PropertyChanged signal for
                // BootProgress. It simply reads the BootProgress from serialize
                // file and updates D-Bus property. Using timer to query the
                // progress to cater fore bmc reboot case.
                auto timerCb = [&bus, &unresolvedRecords,
                                hostPowerOn](Timer& timer) {
                    if (isHostProgressStateRunning(bus))
                    {
                        lg2::info("faultlog poweron timer host reached running "
                                  "state consider it a bmcreboot");
                        createNagPel(
                            bus, unresolvedRecords,
                            !IGNORE_PWR_FAN_PEL); // add as it is bmcreboot
                        timer.setEnabled(false);
                        exit(EXIT_SUCCESS);
                    }
                };
                Timer timer(event, std::move(timerCb), hostStateCheckTimeout);
                timer.setEnabled(true);

                // wait for host to reach runtime
                lg2::info("faultlog host is not in running state create watch "
                          "for progress state");
                std::unique_ptr<sdbusplus::bus::match_t> _hostStatePropWatch =
                    std::make_unique<sdbusplus::bus::match_t>(
                        bus,
                        sdbusplus::bus::match::rules::propertiesChanged(
                            "/xyz/openbmc_project/state/host0",
                            "xyz.openbmc_project.State.Boot."
                            "Progress"),
                        [&bus, &unresolvedRecords, &timer](auto& msg) {
                            propertyChanged(bus, msg, timer);
                        });

                bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);
                return event.loop();
            }
        }
        // write faultlog json to stdout
        else if (listFaultlog)
        {
            (void)FaultLogPolicy::populate(bus, faultLogJson);
            // serviceable event records
            nlohmann::json errorlog = json::array();
            (void)GuardWithEidRecords::populate(bus, unresolvedRecords,
                                                errorlog);
            (void)UnresolvedPELs::populate(bus, unresolvedRecords, errorlog);
            nlohmann::json jsonServiceEvent;
            jsonServiceEvent["SERVICEABLE_EVENT"] = std::move(errorlog);
            faultLogJson.emplace_back(jsonServiceEvent);

            //
            // deconfigured records count
            (void)GuardWithoutEidRecords::populate(unresolvedRecords,
                                                   faultLogJson);
            (void)DeconfigRecords::populate(unresolvedRecords, faultLogJson);
        }
        else
        {
            lg2::error("Invalid option");
        }

        if (listFaultlog || deconfig || unresolvedPels || policy ||
            guardWithoutEid || guardWithEid)
        {
            std::cout << faultLogJson.dump(2) << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed {ERROR}", "ERROR", e.what());
        exit(EXIT_FAILURE);
    }
    // wait for a while for the D-Bus method to complete-
    sleep(2);
    return 0;
}
