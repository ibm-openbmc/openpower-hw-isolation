#include <fmt/format.h>

#include <faultlog_records.hpp>
#include <phosphor-logging/log.hpp>
#include <servicable_records.hpp>

#include <vector>

namespace openpower::faultlog
{
using ::nlohmann::json;
using ::phosphor::logging::level;
using ::phosphor::logging::log;

using AssociationTyple = std::tuple<std::string, std::string, std::string>;
using AssociationsValType = std::vector<AssociationTyple>;

using PropertyValue =
    std::variant<std::string, bool, uint8_t, int16_t, uint16_t, int32_t,
                 uint32_t, int64_t, uint64_t, double, AssociationsValType>;

using Properties = std::map<std::string, PropertyValue>;

using Interfaces = std::map<std::string, Properties>;

/**
 * @class FaultLogRecords
 *
 * Capture all hardware isolation records into the JSON file/
 */
int FaultLogRecords::populate(sdbusplus::bus::bus& bus, json& nagJson,
                              std::vector<int>& /*processedEid*/)
{
    log<level::INFO>("FaultLogRecords::populate()");
    try
    {

        using Objects = std::map<sdbusplus::message::object_path, Interfaces>;

        Objects objects;
        auto method = bus.new_method_call(
            "org.open_power.HardwareIsolation",
            "/xyz/openbmc_project/hardware_isolation",
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
        auto reply = bus.call(method);
        reply.read(objects);

        for (auto& iter : objects)
        {
            std::string prefix = {
                "/xyz/openbmc_project/hardware_isolation/entry/"};
            // could be status object
            std::string object = std::move(std::string(iter.first));
            if (object.find(prefix) != 0)
            {
                continue;
            }
            json jsonErrorLog = json::object();
            log<level::INFO>(
                fmt::format("FaultLogRecords::parse({})", iter.first.str)
                    .c_str());
            for (auto& interface : iter.second)
            {
                if (interface.first == "xyz.openbmc_project.Association."
                                       "Definitions")
                {
                    for (auto& property : interface.second)
                    {
                        if (property.first == "Associations")
                        {
                            const AssociationsValType* associations =
                                std::get_if<AssociationsValType>(
                                    &property.second);
                            if (associations == nullptr)
                            {
                                log<level::ERR>("Failed to get associations");
                                continue;
                            }
                            for (auto& assoc : *associations)
                            {
                                if (std::get<0>(assoc) ==
                                    "isolated_hw_errorlog")
                                {
                                    sdbusplus::message::object_path errPath =
                                        std::get<2>(assoc);
                                    log<level::INFO>(
                                        fmt::format("isolated_hw_errorlog ({})",
                                                    errPath.str)
                                            .c_str());
                                    ServicableRecords::populate(bus, errPath,
                                                                jsonErrorLog);
                                }
                            }
                        }
                    }
                } // else if
            }     // for interfaces
            json jsonEventData = json::object();
            jsonEventData["CEC_ERROR_LOG"] = std::move(jsonErrorLog);
            json jsonServiceEvent = json::object();
            jsonServiceEvent["SERVICABLE_EVENT"] = std::move(jsonEventData);
            nagJson.push_back(std::move(jsonServiceEvent));
        } // for objects
    }
    catch (const std::exception& ex)
    {
        log<level::ERR>(
            fmt::format("Failed to add hardware isolation records ({})",
                        ex.what())
                .c_str());
    }
    return 0;
}
} // namespace openpower::faultlog
