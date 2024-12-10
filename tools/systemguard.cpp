#include "create_pel.hpp"
#include "xyz/openbmc_project/Logging/Entry/server.hpp"

#include <attributes_info.H>

#include <CLI/CLI.hpp>
#include <libguard/guard_interface.hpp>
#include <nlohmann/json.hpp>
#include <xyz/openbmc_project/Logging/Create/server.hpp>

#include <iostream>
extern "C"
{
#include "libpdbg.h"
}
namespace pel = openpower::guard::pel;
std::map<std::string, std::string> guardMap = {
    {"spare", "GARD_Spare"}, {"unrecoverable", "GARD_Unrecoverable"},
    {"fatal", "GARD_Fatal"}, {"predictive", "GARD_Predictive"},
    {"power", "GARD_Power"}, {"phyp", "GARD_PHYP"}};

std::map<std::string, pel::Severity> sevMap = {
    {"GARD_Spare", pel::Severity::Notice},
    {"GARD_Unrecoverable", pel::Severity::Critical},
    {"GARD_Fatal", pel::Severity::Critical},
    {"GARD_Predictive", pel::Severity::Warning},
    {"GARD_Power", pel::Severity::Warning},
    {"GARD_PHYP", pel::Severity::Warning}};

using FFDCFormat =
    sdbusplus::xyz::openbmc_project::Logging::server::Create::FFDCFormat;
/** Data structure to pass to pdbg_target_traverse callback method*/
struct GuardedTarget
{
    pdbg_target* target = nullptr;
    std::string phyDevPath;
    GuardedTarget(const std::string& path) : phyDevPath(path) {}
};

void pdbgLogCallback(int, const char* fmt, va_list ap)
{
    va_list vap;
    va_copy(vap, ap);
    std::vector<char> logData(1 + std::vsnprintf(nullptr, 0, fmt, ap));
    std::vsnprintf(logData.data(), logData.size(), fmt, vap);
    va_end(vap);
    std::string logstr(logData.begin(), logData.end());
    std::cout << "PDBG:" << logstr << std::endl;
}

/**
 * @brief Get PDBG target matching the guarded target physicalpath
 *
 * This callback function is called as part of the recursive method
 * pdbg_target_traverse, recursion will exit when the method return 1
 * else continues till the target is found
 *
 * @param[in] target - pdbg target to compare
 * @param[inout] priv - data structure passed to the callback method
 *
 * @return 1 when target is found else 0
 */
int getGuardedTarget(struct pdbg_target* target, void* priv)
{
    GuardedTarget* guardTarget = reinterpret_cast<GuardedTarget*>(priv);
    ATTR_PHYS_DEV_PATH_Type phyPath;
    if (!DT_GET_PROP(ATTR_PHYS_DEV_PATH, target, phyPath))
    {
        if (strcmp(phyPath, guardTarget->phyDevPath.c_str()) == 0)
        {
            guardTarget->target = target;
            return 1;
        }
    }
    return 0;
}

/**
 *
 * @brief Get the location code of the target to be guarded
 *
 * @param[in] trgt - a pdbg target
 *
 * @return A string containing the FRU location code of the given chip. An empty
 *         string indicates the target was null or the attribute does not exist
 *         for this target.
 */
std::string getLocationCode(pdbg_target* trgt)
{
    if (nullptr == trgt)
    {
        return std::string{};
    }
    ATTR_LOCATION_CODE_Type val;
    if (DT_GET_PROP(ATTR_LOCATION_CODE, trgt, val))
    {
        // Get the immediate parent in the devtree path and try again.
        return getLocationCode(pdbg_target_parent(nullptr, trgt));
    }
    return val;
}

/**
 * @brief convert input physical path to device tree understandable format
 *
 * @param[in] input - physical path of target to be guarded
 *
 * @return Device tree understandable physical path
 **/
std::string getDevTreePhyPathFormat(std::string_view input)
{
    constexpr std::string_view prefix = "physical:";

    // Convert input to lowercase using std::ranges::transform
    std::string lowercase_input(input);
    std::ranges::transform(lowercase_input, lowercase_input.begin(),
                           [](unsigned char c) { return std::tolower(c); });

    // Check if the input starts with "physical:"
    std::string result;
    if (!lowercase_input.starts_with(prefix))
    {
        result = std::string(prefix) + lowercase_input;
    }
    else
    {
        result = std::string(lowercase_input);
    }

    // Check if the result starts with "physical:/"
    constexpr std::string_view unwanted_slash = "physical:/";
    if (result.starts_with(unwanted_slash))
    {
        result.erase(prefix.size(), 1); // Remove the extra slash
    }

    return result;
}

/**
 * @brief Create a PEL with system guard
 *
 * @param[in] guardedTarget - a structure containing the info of the target to
 *be guarded
 * @param[in] sev - severity of the guard
 *
 **/
void createPELWithSystemGuard(struct GuardedTarget& guardedTarget,
                              const std::string sev)
{
    nlohmann::json pelJson;
    ATTR_PHYS_BIN_PATH_Type binPath;
    auto event = "org.open_power.Logging.Error.TestError3";
    pel::FFDCData additionalData;
    pelJson["GuardType"] = guardMap[sev];
    pel::Severity severity = sevMap[guardMap[sev]];
    pelJson["physical_path"] = guardedTarget.phyDevPath;
    pelJson["severity"] = sev;
    pelJson["Guarded"] = true;
    if (!DT_GET_PROP(ATTR_PHYS_BIN_PATH, guardedTarget.target, binPath))
    {
        pelJson["EntityPath"] = binPath;
    }
    pelJson["Priority"] = "H";
    pelJson["LocationCode"] = getLocationCode(guardedTarget.target);
    nlohmann::json pelJsonArr = nlohmann::json::array();
    pelJsonArr.push_back(pelJson);
    try
    {
        pel::FFDCFile file(pelJsonArr);
        int fd = file.getFileFD();
        pel::FFDCInfo ffdcInfo{{FFDCFormat::JSON, static_cast<uint8_t>(0xCA),
                                static_cast<uint8_t>(0x01), fd}};
        pel::createPelWithFFDCfiles(event, additionalData, severity, ffdcInfo);
    }
    catch (const std::exception& ex)
    {
        std::cout << "Failed to create guard" << std::endl;
    }
}

int main(int argc, char** argv)
{
    try
    {
        CLI::App app{"Tool to create system guards"};
        std::string phyDevPath;
        std::optional<std::string> sev;
        app.set_help_flag("-h, --help", "CLI tool options");
        app.add_option("-c, --create", phyDevPath,
                       "Create Guard record, expects physical path as input");
        app.add_option(
            "-s, --severity", sev,
            "Specifies the severity level of the guard "
            "(<Predictive/Fatal/Unrecoverable>). Defaults to Predictive "
            "if no value is provided");
        CLI11_PARSE(app, argc, argv);
        openpower::guard::libguard_init();

        if (phyDevPath.empty())
        {
            std::cout << "Please enter a valid target physical path"
                      << std::endl;
            return -1;
        }

        if (sev)
        {
            std::string severity = *sev;
            std::ranges::transform(
                severity, severity.begin(),
                [](unsigned char c) { return std::tolower(c); });
            *sev = severity;
            if (severity != "predictive" && severity != "fatal")
            {
                std::cout << "Please enter a valid severity" << std::endl;
            }
        }
        else
        {
            *sev = "predictive";
        }

        constexpr auto devtree =
            "/var/lib/phosphor-software-manager/pnor/rw/DEVTREE";

        // PDBG_DTB environment variable set to CEC device tree path
        if (setenv("PDBG_DTB", devtree, 1))
        {
            std::cout << "Failed to set PDBG_DTB: " << strerror(errno)
                      << std::endl;
            return -1;
        }

        // initialize the targeting system
        if (!pdbg_targets_init(NULL))
        {
            std::cout << "pdbg_targets_init failed" << std::endl;
            return -1;
        }

        // set log level and callback function
        pdbg_set_loglevel(PDBG_DEBUG);
        pdbg_set_logfunc(pdbgLogCallback);
        GuardedTarget guardedTarget(getDevTreePhyPathFormat(phyDevPath));
        auto ret = pdbg_target_traverse(nullptr, getGuardedTarget,
                                        &guardedTarget);
        if (ret == 0)
        {
            std::cout << "Please enter a valid physical path" << std::endl;
            return -1;
        }
        std::cout << "Creating System guard of type " << *sev
                  << " on the target with physical path " << phyDevPath
                  << std::endl;
        createPELWithSystemGuard(guardedTarget, *sev);
    }
    catch (const std::exception& ex)
    {
        std::cout << "Exception: " << ex.what() << std::endl;
    }
    return 0;
}
