// SPDX-License-Identifier: Apache-2.0

#include "common/utils.hpp"

#include "common/phal_devtree_utils.hpp"

#include <xyz/openbmc_project/State/Chassis/server.hpp>

namespace hw_isolation
{
namespace utils
{

void initExternalModules()
{
    devtree::initPHAL();

    // Don't initialize the phal device tree again, it will init through
    // devtree::initPHAL because, phal device tree should initialize
    // only once (as per pdbg expectation) in single process context.
    // so, passing as false to libguard_init.
    openpower_guard::libguard::libguard_init(false);
}

std::string getDBusServiceName(sdbusplus::bus::bus& bus,
                               const std::string& path,
                               const std::string& interface)
{
    std::vector<std::pair<std::string, std::vector<std::string>>> servicesName;

    try
    {
        auto method =
            bus.new_method_call(type::ObjectMapperName, type::ObjectMapperPath,
                                type::ObjectMapperName, "GetObject");

        method.append(path);
        method.append(std::vector<std::string>({interface}));

        auto reply = bus.call(method);
        reply.read(servicesName);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(fmt::format("Exception [{}] to get dbus service name "
                                    "for object [{}] and interface [{}]",
                                    e.what(), path, interface)
                            .c_str());
        throw sdbusplus::exception::SdBusError(
            const_cast<sd_bus_error*>(e.get_error()), "HW-Isolation");
    }

    // In OpenBMC, the object path will be hosted by a single service
    // i.e more than one service cannot host the same object path.
    if (servicesName.size() > 1)
    {
        std::string serviceNameList{""};

        std::for_each(servicesName.begin(), servicesName.end(),
                      [&serviceNameList](const auto& serviceName) {
                          serviceNameList.append(serviceName.first + ",");
                      });

        log<level::ERR>(fmt::format("The given object path hosted by "
                                    "more than one services [{}]",
                                    serviceNameList)
                            .c_str());
        throw std::runtime_error(
            "Given object path hosted by more than one service");
    }

    return servicesName[0].first;
}

bool isHwIosolationSettingEnabled(sdbusplus::bus::bus& bus)
{
    try
    {
        return utils::getDBusPropertyVal<bool>(
            bus, "/xyz/openbmc_project/hardware_isolation/allow_hw_isolation",
            "xyz.openbmc_project.Object.Enable", "Enabled");
    }
    catch (const std::exception& e)
    {
        // Log is already added in getDBusPropertyVal()
        // By default, the HardwareIsolation feature is need to allow
        return true;
    }
}

void isHwDeisolationAllowed(sdbusplus::bus::bus& bus)
{
    // Make sure the hardware isolation setting is enabled or not
    if (!isHwIosolationSettingEnabled(bus))
    {
        log<level::INFO>(
            fmt::format("Hardware deisolation is not allowed "
                        "since the HardwareIsolation setting is disabled")
                .c_str());
        throw type::CommonError::Unavailable();
    }

    using Chassis = sdbusplus::xyz::openbmc_project::State::server::Chassis;

    auto systemPowerState = utils::getDBusPropertyVal<std::string>(
        bus, "/xyz/openbmc_project/state/chassis0",
        "xyz.openbmc_project.State.Chassis", "CurrentPowerState");

    if (Chassis::convertPowerStateFromString(systemPowerState) !=
        Chassis::PowerState::Off)
    {
        log<level::ERR>(fmt::format("Manual hardware de-isolation is allowed "
                                    "only when chassis powerstate is off")
                            .c_str());
        throw type::CommonError::NotAllowed();
    }
}

void setEnabledProperty(sdbusplus::bus::bus& bus,
                        const std::string& dbusObjPath, bool enabledPropVal)
{
    /**
     * Make sure "Object::Enable" interface is implemented for the given
     * dbus object path and don't throw an exception if the interface or
     * property "Enabled" is not implemented since "Enabled" property
     * update requires only for few hardware which are going isolate from
     * external interface i.e Redfish
     */
    constexpr auto enabledPropIface = "xyz.openbmc_project.Object.Enable";

    // Using two try and catch block to avoid more trace for same issue
    // since using common utils API "setDBusPropertyVal"
    try
    {
        getDBusServiceName(bus, dbusObjPath, enabledPropIface);
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
        setDBusPropertyVal<bool>(bus, dbusObjPath, enabledPropIface, "Enabled",
                                 enabledPropVal);
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

std::optional<sdbusplus::message::object_path>
    getBMCLogPath(sdbusplus::bus::bus& bus, const uint32_t eid)
{
    // If EID is "0" means, no bmc error log.
    if (eid == 0)
    {
        return sdbusplus::message::object_path();
    }

    try
    {
        auto dbusServiceName = utils::getDBusServiceName(
            bus, type::LoggingObjectPath, type::LoggingInterface);

        auto method = bus.new_method_call(
            dbusServiceName.c_str(), type::LoggingObjectPath,
            type::LoggingInterface, "GetBMCLogIdFromPELId");

        method.append(static_cast<uint32_t>(eid));
        auto resp = bus.call(method);

        uint32_t bmcLogId;
        resp.read(bmcLogId);

        return sdbusplus::message::object_path(
            std::string(type::LoggingObjectPath) + "/entry/" +
            std::to_string(bmcLogId));
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(
            fmt::format("Exception [{}] when trying to get BMC log path "
                        "for the given EID (aka PEL ID) [{}]",
                        e.what(), eid)
                .c_str());
        return std::nullopt;
    }
}

std::optional<type::InstanceId> getInstanceId(const std::string& objPathSegment)
{
    try
    {
        /**
         * FIXME: The assumption is, the instance id (numberic value) will be
         *        always added at last in the object path segment
         *        for the OpenBMC.
         */
        auto it =
            std::find_if_not(objPathSegment.crbegin(), objPathSegment.crend(),
                             [](const char chr) { return std::isdigit(chr); });

        type::InstanceId hwInstanceId{type::Invalid_InstId};
        if (it != objPathSegment.crbegin())
        {
            hwInstanceId = std::stoi(objPathSegment.substr(
                std::distance(it, objPathSegment.crend())));
        }
        return hwInstanceId;
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(fmt::format("Exception [{}] to get instance id from "
                                    "the given D-Bus object path segment [{}]",
                                    e.what(), objPathSegment)
                            .c_str());
    }
    return std::nullopt;
}

} // namespace utils
} // namespace hw_isolation
