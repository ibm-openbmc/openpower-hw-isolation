// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common_types.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>

namespace hw_isolation
{
namespace utils
{

using namespace phosphor::logging;

/**
 * @brief API to initialize external modules (libraries)
 *
 * @return Nothing on success
 *         Throw exception on failure.
 */
void initExternalModules();

/**
 * @brief Get the Dbus service name
 *
 * @param[in] bus - Bus to attach to.
 * @param[in] objPath - Dbus object path.
 * @param[in] interface - Dbus interface name.
 *
 * @return the service name as string on success
 *         throw exception on failure.
 */
std::string getDBusServiceName(sdbusplus::bus::bus& bus,
                               const std::string& objPath,
                               const std::string& interface);

/**
 * @brief Get the given dbus property value
 *
 * @param[in] bus - Bus to attach to.
 * @param[in] objPath - Dbus object path.
 * @param[in] propInterface - Interface name of property.
 * @param[in] propName - Name of property to get value.
 *
 * @return The dbus property value as T type on success
 *         throw exception on failure.
 *
 * @note The caller must take care the value validation
 *       i.e the given value is empty or not.
 */
template <typename T>
T getDBusPropertyVal(sdbusplus::bus::bus& bus, const std::string& objPath,
                     const std::string& propInterface,
                     const std::string& propName)
{

    T propertyVal;
    try
    {
        auto dbusServiceName = getDBusServiceName(bus, objPath, propInterface);

        auto method =
            bus.new_method_call(dbusServiceName.c_str(), objPath.c_str(),
                                "org.freedesktop.DBus.Properties", "Get");

        method.append(propInterface, propName);

        auto reply = bus.call(method);

        std::variant<T> resp;
        reply.read(resp);
        propertyVal = std::get<T>(resp);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(
            fmt::format("Exception [{}] to get the given dbus property "
                        "[{}] interface [{}] for object path [{}]",
                        e.what(), propName, propInterface, objPath)
                .c_str());
        throw sdbusplus::exception::SdBusError(
            const_cast<sd_bus_error*>(e.get_error()), "HW-Isolation");
    }
    catch (const std::bad_variant_access& e)
    {
        log<level::ERR>(
            fmt::format("Exception [{}] to get the given dbus property "
                        "[{}] interface [{}] for object path [{}]",
                        e.what(), propName, propInterface, objPath)
                .c_str());
        throw e;
    }

    return propertyVal;
}

/**
 * @brief Set the given dbus property value
 *
 * @param[in] bus - Bus to attach to.
 * @param[in] objPath - Dbus object path.
 * @param[in] propInterface - Interface name of property.
 * @param[in] propName - Name of property to set value.
 *
 * @return NULL on success
 *         throw exception on failure.
 */
template <typename T>
void setDBusPropertyVal(sdbusplus::bus::bus& bus, const std::string& objPath,
                        const std::string& propInterface,
                        const std::string& propName, const T& propVal)
{

    try
    {
        auto dbusServiceName = getDBusServiceName(bus, objPath, propInterface);

        auto method =
            bus.new_method_call(dbusServiceName.c_str(), objPath.c_str(),
                                "org.freedesktop.DBus.Properties", "Set");

        std::variant<T> propertyVal{propVal};
        method.append(propInterface, propName, propertyVal);

        auto reply = bus.call(method);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(
            fmt::format("Exception [{}] to set the given dbus property "
                        "[{}] interface [{}] for object path [{}]",
                        e.what(), propName, propInterface, objPath)
                .c_str());
        throw sdbusplus::exception::SdBusError(
            const_cast<sd_bus_error*>(e.get_error()), "HW-Isolation");
    }
}

/**
 * @brief Used to get to know whether hardware deisolation is allowed
 *
 * @param[in] bus - Bus to attach to.
 *
 * @return Throw appropriate exception if not allowed
 *         NULL if allowed
 */
void isHwDeisolationAllowed(sdbusplus::bus::bus& bus);

/**
 * @brief Used to get to know whether hardware isolation setting is
 *        enabled or not
 *
 * @param[in] bus - Bus to attach to.
 *
 * @return The hardware isolation setting on success
 *         true to allow the hardware isolation feature on failure
 */
bool isHwIosolationSettingEnabled(sdbusplus::bus::bus& bus);

/**
 * @brief Used to set the Enabled property value by using the given
 *        dbus object path
 *
 * @param[in] bus - Bus to attach to.
 * @param[in] dbusObjPath - The object path to set enabled property value
 * @param[in] enabledPropVal - set the enabled property value
 *
 * @return NULL on success
 *         Throw exception on failure
 *
 * @note It will set enabled property value if found the enabled
 *       property in the given object path. If not found then it will just
 *       add the trace and won't throw exception.
 */
void setEnabledProperty(sdbusplus::bus::bus& bus,
                        const std::string& dbusObjPath, bool enabledPropVal);

/**
 * @brief Used to get BMC log object path by using EID (aka PEL ID)
 *
 * @param[in] bus - Bus to attach to.
 * @param[in] eid - The EID (aka PEL ID) to get BMC log object path
 *
 * @return The BMC log object path
 *         Empty optional on failure
 */
std::optional<sdbusplus::message::object_path>
    getBMCLogPath(sdbusplus::bus::bus& bus, const uint32_t eid);

/**
 * @brief Helper function to get the instance id from the given
 *        D-Bus object path segment.
 *        Example: core0 -> 0
 *
 * @param[in] objPathSegment - The D-Bus object path segment to get
 *                             the instance id
 *
 * @return The instance id on success
 *         Empty optional on failure
 *
 * @note This API will return the invalid instance id if the given object
 *       path segment doesn't contain (any digit) the instance id because
 *       some of the D-Bus objects path segment doesn't have instance id.
 *       For example, TPM inventory object path.
 */
std::optional<type::InstanceId>
    getInstanceId(const std::string& objPathSegment);

} // namespace utils
} // namespace hw_isolation
