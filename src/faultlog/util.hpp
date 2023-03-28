#pragma once
#include <fmt/format.h>

#include <phosphor-logging/log.hpp>

namespace openpower::faultlog
{
using ::phosphor::logging::level;
using ::phosphor::logging::log;

/**
 * @brief Read property value from the specified object and interface
 * @param[in] bus D-Bus handle
 * @param[in] service service which has implemented the interface
 * @param[in] object object having has implemented the interface
 * @param[in] intf interface having the property
 * @param[in] prop name of the property to read
 * @return property value
 */
template <typename T>
T readProperty(sdbusplus::bus::bus& bus, const std::string& service,
               const std::string& object, const std::string& intf,
               const std::string& prop)
{
    T retVal{};
    try
    {
        auto properties =
            bus.new_method_call(service.c_str(), object.c_str(),
                                "org.freedesktop.DBus.Properties", "Get");
        properties.append(intf);
        properties.append(prop);
        auto result = bus.call(properties);
        result.read(retVal);
    }
    catch (const std::exception& ex)
    {
        log<level::ERR>(
            fmt::format("Failed to get the property ({}) interface ({}) "
                        "object path ({}) error ({}) ",
                        prop.c_str(), intf.c_str(), object.c_str(), ex.what())
                .c_str());
        throw;
    }
    return retVal;
}

} // namespace openpower::faultlog
