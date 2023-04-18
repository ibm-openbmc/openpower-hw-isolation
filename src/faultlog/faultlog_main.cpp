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
#include <unresolved_pels.hpp>
#include <util.hpp>

#include <iostream>
#include <vector>

using namespace openpower::faultlog;
using ::nlohmann::json;
using ::openpower::guard::GuardRecords;

#define GUARD_RESOLVED 0xFFFFFFFF

using Severity = sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;

using Binary = std::vector<uint8_t>;
using PropVariant = sdbusplus::utility::dedup_variant_t<Binary>;
int main(int argc, char** argv)
{
    try
    {
        lg2::info("faultlog app to collect deconfig/guard records details");

        CLI::App app{"Faultlog tool"};
        app.set_help_flag("-h, --help", "Faultlog tool options");

        auto bus = sdbusplus::bus::new_default();

        nlohmann::json faultLogJson = json::array();

        std::string propVal{};
        try
        {
            auto retVal = readProperty<PropVariant>(
                bus, "xyz.openbmc_project.Inventory.Manager",
                "/xyz/openbmc_project/inventory/system/chassis/motherboard",
                "com.ibm.ipzvpd.VSYS", "TM");
            if (auto pVal = std::get_if<Binary>(&retVal))
            {
                propVal.assign(reinterpret_cast<const char*>(pVal->data()),
                               pVal->size());
            }
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

        openpower::guard::libguard_init();
        // Don't get ephemeral records because those type records are not
        // intended to expose to the end user, just created for internal purpose
        // to use by the BMC and Hostboot.
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

        bool guardWithEid = false;
        bool guardWithoutEid = false;
        bool policy = false;
        bool unresolvedPels = false;
        bool deconfig = false;
        bool createPel = false;
        bool listFaultlog = false;

        app.set_help_flag("-h, --help", "Faultlog tool options");
        app.add_flag("-g, --guardwterr", guardWithEid,
                     "Populate guard records with associated error objects "
                     "details to JSON");
        app.add_flag("-m, --guardmanual", guardWithoutEid,
                     "Populate guard records without associated error objects "
                     "details to JSON");
        app.add_flag("-p, --policy", policy,
                     "Populate faultlog policy and FCO values to JSON");
        app.add_flag(
            "-u, --unresolvedPels", unresolvedPels,
            "Populate unresolved pels with deconfig bit set details to JSON");
        app.add_flag("-d, --deconfig", deconfig,
                     "Populate deconfigured target details to JSON");
        app.add_flag("-c, --createPel", createPel,
                     "Create faultlog pel if there are guarded/deconfigured "
                     "records present");
        app.add_flag("-f, --faultlog", listFaultlog,
                     "List all fault log records in JSON format");

        CLI11_PARSE(app, argc, argv);

        // guard records with associated error object
        if (guardWithEid)
        {
            (void)GuardWithEidRecords::populate(bus, unresolvedRecords,
                                                faultLogJson);
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
            (void)UnresolvedPELs::populate(bus, unresolvedRecords,
                                           faultLogJson);
        }

        // pdbg targets with deconfig bit set
        else if (deconfig)
        {
            (void)DeconfigRecords::populate(faultLogJson);
        }

        // create fault log pel if there are service actions pending
        else if (createPel)
        {
            int guardCount = GuardWithEidRecords::getCount(unresolvedRecords);
            int manualGuardCount =
                GuardWithoutEidRecords::getCount(unresolvedRecords);
            int unresolvedPelsCount = UnresolvedPELs::getCount(bus);
            int deconfigCount = DeconfigRecords::getCount();

            if ((guardCount > 0) || (manualGuardCount > 0) ||
                (unresolvedPelsCount > 0) || (deconfigCount > 0))
            {
                std::unordered_map<std::string, std::string> data;
                data.emplace("GUARD_WITH_ASSOC_ERROR_COUNT",
                             std::to_string(guardCount));
                data.emplace("GUARD_WITH_NO_ASSOC_ERROR_COUNT",
                             std::to_string(manualGuardCount));
                data.emplace("UNRESOLVED_PEL_WITH_DECONFIG_BIT_COUNT",
                             std::to_string(unresolvedPelsCount));
                data.emplace("DECONFIG_RECORD_COUNT",
                             std::to_string(deconfigCount));

                auto method = bus.new_method_call(
                    "xyz.openbmc_project.Logging",
                    "/xyz/openbmc_project/logging",
                    "xyz.openbmc_project.Logging.Create", "Create");
                method.append("org.open_power.Faultlog.Error.DeconfiguredHW",
                              Severity::Error, data);
                auto reply = method.call();
                if (reply.is_method_error())
                {
                    lg2::error("Error in calling D-Bus method to create PEL");
                }
                lg2::info("faultlog {GUARD_COUNT}, {MAN_GUARD_COUNT}, "
                          "{DECONFIG_COUNT} , {PEL_COUNT} ",
                          "GUARD_COUNT", guardCount, "MAN_GUARD_COUNT",
                          manualGuardCount, "DECONFIG_COUNT", deconfigCount,
                          "PEL_COUNT", unresolvedPelsCount);
            }
            else
            {
                lg2::info("There are no pending service actions ignoring "
                          "creating fautlog pel");
            }
        }
        // write faultlog json to stdout
        else if (listFaultlog)
        {
            (void)GuardWithEidRecords::populate(bus, unresolvedRecords,
                                                faultLogJson);
            (void)GuardWithoutEidRecords::populate(unresolvedRecords,
                                                   faultLogJson);

            (void)FaultLogPolicy::populate(bus, faultLogJson);

            (void)UnresolvedPELs::populate(bus, unresolvedRecords,
                                           faultLogJson);
            (void)DeconfigRecords::populate(faultLogJson);
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
