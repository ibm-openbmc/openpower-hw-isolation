#include <fmt/format.h>

#include <faultlog_policy.hpp>
#include <phosphor-logging/log.hpp>
#include <util.hpp>

namespace openpower::faultlog
{
using ::nlohmann::json;
using ::phosphor::logging::level;
using ::phosphor::logging::log;

/**
 * @class FaultLogPolicy
 *
 * Capture hardware isolation policy and FCO value in NAG json file
 */
int FaultLogPolicy::populate(sdbusplus::bus::bus& bus, nlohmann::json& nagJson)
{
    log<level::INFO>("FaultLogPolicy::populate()");
    try
    {
        json jsonPolicyVal = json::object();
        // FCO_VALUE
        auto method = bus.new_method_call(
            "xyz.openbmc_project.BIOSConfigManager",
            "/xyz/openbmc_project/bios_config/manager",
            "xyz.openbmc_project.BIOSConfig.Manager", "GetAttribute");
        method.append("hb_field_core_override");

        std::tuple<std::string, std::variant<int64_t, std::string>,
                   std::variant<int64_t, std::string>>
            attrVal;
        auto result = bus.call(method);
        result.read(std::get<0>(attrVal), std::get<1>(attrVal),
                    std::get<2>(attrVal));

        std::variant<int64_t, std::string> attr = std::get<1>(attrVal);
        uint32_t fcoVal = 0;
        if (auto pVal = std::get_if<int64_t>(&attr))
        {
            fcoVal = *pVal;
        }
        jsonPolicyVal["FCO_VALUE"] = fcoVal;

        // master guard enabled or not
        bool enabled = readProperty<bool>(
            bus, "xyz.openbmc_project.Settings",
            "/xyz/openbmc_project/hardware_isolation/allow_hw_isolation",
            "xyz.openbmc_project.Object.Enable", "Enabled");
        jsonPolicyVal["MASTER"] = enabled;

        // predictive guard enabled or not
        // at present not present in BMC will leave it as true for now
        jsonPolicyVal["PREDICTIVE"] = true;

        json jsonPolicy = json::object();
        jsonPolicy["POLICY"] = std::move(jsonPolicyVal);
        nagJson.push_back(std::move(jsonPolicy));
    }
    catch (const std::exception& ex)
    {
        log<level::ERR>(
            fmt::format("Failed to add policy details to NAG ({})", ex.what())
                .c_str());
    }

    return 0;
}
} // namespace openpower::faultlog
