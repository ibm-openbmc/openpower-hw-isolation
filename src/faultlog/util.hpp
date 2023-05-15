#pragma once

#include <libguard/include/guard_record.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/State/Boot/Progress/server.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>
using ::openpower::guard::GuardRecords;

namespace openpower::faultlog
{

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
        lg2::error(
            "Failed to read property: {PROPERTY}, {INTF}, {PATH}, {ERROR}",
            "PROPERTY", prop, "INTF", intf, "PATH", object, "ERROR", ex.what());
        throw;
    }
    return retVal;
}

/**
 * @brief get the guard reason for the specified physical path
 *        of the pdbg target
 * @param[in] guardRecords - guard records
 * @param[in] path - physical path of the pdbg target
 *
 * @return guard reason stored as part of the guard record
 */
std::string getGuardReason(const GuardRecords& guardRecords,
                           const std::string& path);

/**
 * @brief Return true if host completed IPL and reached runtime
 * @param[in] bus - D-Bus handle
 *
 * @return true if in runtime else false
 */
bool isHostProgressStateRunning(sdbusplus::bus::bus& bus);

/**
 * @brief Return true if host started running
 * @param[in] bus - D-Bus handle
 *
 * @return true if host started
 */
bool isHostStateRunning(sdbusplus::bus::bus& bus);
} // namespace openpower::faultlog
