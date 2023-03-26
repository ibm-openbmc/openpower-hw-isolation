#include <fmt/format.h>

#include <phosphor-logging/log.hpp>
#include <servicable_records.hpp>

namespace openpower::faultlog
{
using ::nlohmann::json;
using ::phosphor::logging::level;
using ::phosphor::logging::log;

void ServicableRecords::populate(sdbusplus::bus::bus& bus,
                                 sdbusplus::message::object_path& errorLog,
                                 json& jsonServEvent)
{
    log<level::INFO>(
        fmt::format("parseErrorlogObjectPath errorlog ({})", errorLog.str)
            .c_str());
    std::string pel;
    auto method = bus.new_method_call(
        "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
        "org.open_power.Logging.PEL", "GetPELJSON");
    method.append(static_cast<uint32_t>(std::stoi(errorLog.filename())));
    auto resp = bus.call(method);
    resp.read(pel);
    json pelJson = std::move(json::parse(pel));

    jsonServEvent["ERR_PLID"] = pelJson["Private Header"]["Platform Log Id"];
    jsonServEvent["Callout Section"] =
        pelJson["Primary SRC"]["Callout Section"];
    jsonServEvent["SRC"] = pelJson["Primary SRC"]["Reference Code"];
    jsonServEvent["DATE_TIME"] = pelJson["Private Header"]["Created at"];
}
} // namespace openpower::faultlog
