// SPDX-License-Identifier: Apache-2.0

#include "hardware_isolation_manager.hpp"

#include "utils.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>

namespace hw_isolation
{

using namespace phosphor::logging;

Manager::Manager(sdbusplus::bus::bus& bus) : _bus(bus)
{}

std::optional<uint32_t>
    Manager::getEID(const sdbusplus::message::object_path& bmcErrorLog) const
{
    try
    {
        constexpr auto loggingObjectPath = "/xyz/openbmc_project/logging";
        auto loggingInterface = "org.open_power.Logging.PEL";
        uint32_t eid;

        auto dbusServiceName = utils::getDBusServiceName(
            _bus, loggingObjectPath, loggingInterface);

        auto method =
            _bus.new_method_call(dbusServiceName.c_str(), loggingObjectPath,
                                 loggingInterface, "GetPELIdFromBMCLogId");

        method.append(static_cast<uint32_t>(std::stoi(bmcErrorLog.filename())));
        auto resp = _bus.call(method);

        resp.read(eid);
        return eid;
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(fmt::format("Exception [{}] to get EID (aka PEL ID) "
                                    "for object [{}]",
                                    e.what(), bmcErrorLog.str)
                            .c_str());
    }
    return std::nullopt;
}

} // namespace hw_isolation
