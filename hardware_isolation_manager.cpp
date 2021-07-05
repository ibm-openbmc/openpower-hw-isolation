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

void Manager::setAvailableProperty(const std::string& dbusObjPath,
                                   bool availablePropVal)
{
    /**
     * Make sure "Availablity" interface is implemented for the given
     * dbus object path and don't throw an exception if the interface or
     * property "Available" is not implemented since "Available" property
     * update requires only for few hardware which are going isolate from
     * external interface i.e Redfish
     */
    constexpr auto availablityIface =
        "xyz.openbmc_project.State.Decorator.Availablity";

    // Using two try and catch block to avoid more trace for same issue
    // since using common utils API "setDBusPropertyVal"
    try
    {
        utils::getDBusServiceName(_bus, dbusObjPath, availablityIface);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        if (std::string(e.name()) ==
            std::string("xyz.openbmc_project.Common.Error.ResourceNotFound"))
        {
            return;
        }
        throw sdbusplus::exception::SdBusError(
            const_cast<sd_bus_error*>(e.get_error()), "HW-Isolation");
    }

    try
    {
        utils::setDBusPropertyVal<bool>(_bus, dbusObjPath, availablityIface,
                                        "Available", availablePropVal);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        if (std::string(e.name()) ==
            std::string("org.freedesktop.DBus.Error.UnknownProperty"))
        {
            return;
        }
        throw sdbusplus::exception::SdBusError(
            const_cast<sd_bus_error*>(e.get_error()), "HW-Isolation");
    }
}

} // namespace hw_isolation
