#include <faultlog_policy.hpp>
#include <phosphor-logging/lg2.hpp>
#include <service_alerts_time.hpp>
#include <util.hpp>

namespace openpower::faultlog
{
using ::nlohmann::json;

void FaultLogPolicy::populate(sdbusplus::bus::bus& bus, nlohmann::json& nagJson)
{
    try
    {
        json jsonPolicyVal = json::object();
        // FCO_VALUE
        auto method = bus.new_method_call(
            "xyz.openbmc_project.BIOSConfigManager",
            "/xyz/openbmc_project/bios_config/manager",
            "xyz.openbmc_project.BIOSConfig.Manager", "GetAttribute");
        method.append("hb_field_core_override_current");

        std::tuple<std::string, std::variant<int64_t, std::string>,
                   std::variant<int64_t, std::string>>
            fcoAttrVal;
        auto result = bus.call(method);
        result.read(std::get<0>(fcoAttrVal), std::get<1>(fcoAttrVal),
                    std::get<2>(fcoAttrVal));

        std::variant<int64_t, std::string> attr = std::get<1>(fcoAttrVal);
        uint32_t fcoVal = 0;
        if (auto pVal = std::get_if<int64_t>(&attr))
        {
            fcoVal = *pVal;
        }
        jsonPolicyVal["FCO_VALUE"] = fcoVal;

        bool enabled = true;
        try
        {
            // master guard enabled or not
            enabled = readProperty<bool>(
                bus, "xyz.openbmc_project.Settings",
                "/xyz/openbmc_project/hardware_isolation/allow_hw_isolation",
                "xyz.openbmc_project.Object.Enable", "Enabled");
        }
        catch (const std::exception& ex)
        {
            lg2::info("Failed to read allow_hw_isolation property {ERROR}",
                      "ERROR", ex);
        }
        jsonPolicyVal["MASTER"] = enabled;

        // predictive guard enabled or not
        // at present not present in BMC will leave it as true for now
        jsonPolicyVal["PREDICTIVE"] = true;

        // service alerts enabled or not
        bool serviceAlertsEnabled = true;
        try
        {
            serviceAlertsEnabled = readProperty<bool>(
                bus, "xyz.openbmc_project.Settings",
                "/xyz/openbmc_project/hardware_isolation/send_service_alerts",
                "xyz.openbmc_project.Object.Enable", "Enabled");
        }
        catch (const std::exception& ex)
        {
            lg2::info("Failed to read send_service_alerts property {ERROR}",
                      "ERROR", ex);
        }
        jsonPolicyVal["SEND_SERVICE_ALERTS"] = serviceAlertsEnabled;

        json jsonPolicy = json::object();
        jsonPolicy["POLICY"] = std::move(jsonPolicyVal);
        nagJson.push_back(std::move(jsonPolicy));
    }
    catch (const std::exception& ex)
    {
        lg2::error("Failed to add isolation policy details to JSON {ERROR}",
                   "ERROR", ex);
    }

    return;
}
} // namespace openpower::faultlog
