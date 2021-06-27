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
} // namespace utils
} // namespace hw_isolation
