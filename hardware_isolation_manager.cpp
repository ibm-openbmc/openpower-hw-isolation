// SPDX-License-Identifier: Apache-2.0

#include "config.h"

#include "hardware_isolation_manager.hpp"

#include "common_types.hpp"
#include "openpower_guard_interface.hpp"
#include "utils.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/State/Chassis/server.hpp>

#include <filesystem>

namespace hw_isolation
{

using namespace phosphor::logging;
namespace fs = std::filesystem;

constexpr auto loggingObjectPath = "/xyz/openbmc_project/logging";
constexpr auto loggingInterface = "org.open_power.Logging.PEL";

Manager::Manager(sdbusplus::bus::bus& bus, const std::string& objPath) :
    type::ServerObject<CreateInterface, DeleteAllInterface>(
        bus, objPath.c_str(), true),
    _bus(bus), _lastEntryId(0), _isolatableHWs(bus)
{}

std::optional<uint32_t>
    Manager::getEID(const sdbusplus::message::object_path& bmcErrorLog) const
{
    try
    {
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

std::optional<sdbusplus::message::object_path> Manager::createEntry(
    const entry::EntryRecordId& recordId, const entry::EntryResolved& resolved,
    const entry::EntrySeverity& severity, const std::string& isolatedHardware,
    const std::string& bmcErrorLog, const bool deleteRecord)
{
    try
    {
        auto id = _lastEntryId + 1;
        auto entryObjPath =
            fs::path(HW_ISOLATION_ENTRY_OBJPATH) / std::to_string(id);

        // Add association for isolated hardware inventory path
        // Note: Association forward and reverse type are defined as per
        // hardware isolation design document (aka guard) and hardware isolation
        // entry dbus interface document for hardware and error object path
        entry::AsscDefFwdType isolateHwFwdType("isolated_hw");
        entry::AsscDefRevType isolatedHwRevType("isolated_hw_entry");
        entry::AssociationDef associationDeftoHw;
        associationDeftoHw.push_back(std::make_tuple(
            isolateHwFwdType, isolatedHwRevType, isolatedHardware));

        // Add errog log as Association if given
        if (!bmcErrorLog.empty())
        {
            entry::AsscDefFwdType bmcErrorLogFwdType("isolated_hw_errorlog");
            entry::AsscDefRevType bmcErrorLogRevType("isolated_hw_entry");
            associationDeftoHw.push_back(std::make_tuple(
                bmcErrorLogFwdType, bmcErrorLogRevType, bmcErrorLog));
        }

        _isolatedHardwares.insert(std::make_pair(
            id, std::make_unique<entry::Entry>(_bus, entryObjPath, id, recordId,
                                               severity, resolved,
                                               associationDeftoHw)));

        setAvailableProperty(isolatedHardware, false);

        // Update the last entry id by using the created entry id.
        _lastEntryId = id;
        return entryObjPath.string();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(
            fmt::format("Exception [{}], so failed to create entry", e.what())
                .c_str());

        if (deleteRecord)
        {
            openpower_guard::clear(recordId);
        }
    }
    return std::nullopt;
}

void Manager::isHwIsolationAllowed(const entry::EntrySeverity& severity)
{
    if (severity == entry::EntrySeverity::Manual)
    {
        using Chassis = sdbusplus::xyz::openbmc_project::State::server::Chassis;

        auto systemPowerState = utils::getDBusPropertyVal<std::string>(
            _bus, "/xyz/openbmc_project/state/chassis0",
            "xyz.openbmc_project.State.Chassis", "CurrentPowerState");

        if (Chassis::convertPowerStateFromString(systemPowerState) !=
            Chassis::PowerState::Off)
        {
            log<level::ERR>(fmt::format("Manual hardware isolation is allowed "
                                        "only when chassis powerstate is off")
                                .c_str());
            throw type::CommonError::NotAllowed();
        }
    }
}

sdbusplus::message::object_path Manager::create(
    sdbusplus::message::object_path isolateHardware,
    sdbusplus::xyz::openbmc_project::HardwareIsolation::server::Entry::Type
        severity)
{
    isHwIsolationAllowed(severity);

    auto devTreePhysicalPath = _isolatableHWs.getPhysicalPath(isolateHardware);
    if (!devTreePhysicalPath.has_value())
    {
        log<level::ERR>(fmt::format("Invalid argument [IsolateHardware: {}]",
                                    isolateHardware.str)
                            .c_str());
        throw type::CommonError::InvalidArgument();
    }

    auto guardRecord = openpower_guard::create(
        devTreePhysicalPath->data(), 0, entry::utils::getGuardType(severity));

    auto entryPath = createEntry(guardRecord->recordId, false, severity,
                                 isolateHardware.str, "", true);

    if (!entryPath.has_value())
    {
        throw type::CommonError::InternalFailure();
    }
    return *entryPath;
}

sdbusplus::message::object_path Manager::createWithErrorLog(
    sdbusplus::message::object_path isolateHardware,
    sdbusplus::xyz::openbmc_project::HardwareIsolation::server::Entry::Type
        severity,
    sdbusplus::message::object_path bmcErrorLog)
{
    isHwIsolationAllowed(severity);

    auto devTreePhysicalPath = _isolatableHWs.getPhysicalPath(isolateHardware);
    if (!devTreePhysicalPath.has_value())
    {
        log<level::ERR>(fmt::format("Invalid argument [IsolateHardware: {}]",
                                    isolateHardware.str)
                            .c_str());
        throw type::CommonError::InvalidArgument();
    }

    auto eId = getEID(bmcErrorLog);
    if (!eId.has_value())
    {
        log<level::ERR>(
            fmt::format("Invalid argument [BmcErrorLog: {}]", bmcErrorLog.str)
                .c_str());
        throw type::CommonError::InvalidArgument();
    }

    auto guardRecord =
        openpower_guard::create(devTreePhysicalPath->data(), *eId,
                                entry::utils::getGuardType(severity));

    auto entryPath = createEntry(guardRecord->recordId, false, severity,
                                 isolateHardware.str, bmcErrorLog.str, true);

    if (!entryPath.has_value())
    {
        throw type::CommonError::InternalFailure();
    }
    return *entryPath;
}

void Manager::deleteAll()
{
    auto entryIt = _isolatedHardwares.begin();
    while (entryIt != _isolatedHardwares.end())
    {
        auto& entry = entryIt->second;
        std::advance(entryIt, 1);
        entry->delete_();
    }
}

std::optional<sdbusplus::message::object_path>
    Manager::getBMCLogPath(const uint32_t eid) const
{
    // If EID is "0" means doesn't have associated error log in
    // isolated hardware
    if (eid == 0)
    {
        return sdbusplus::message::object_path();
    }

    try
    {
        auto dbusServiceName = utils::getDBusServiceName(
            _bus, loggingObjectPath, loggingInterface);

        auto method =
            _bus.new_method_call(dbusServiceName.c_str(), loggingObjectPath,
                                 loggingInterface, "GetBMCLogIdFromPELId");

        method.append(static_cast<uint32_t>(eid));
        auto resp = _bus.call(method);

        uint32_t bmcLogId;
        resp.read(bmcLogId);

        return sdbusplus::message::object_path(std::string(loggingObjectPath) +
                                               "/entry/" +
                                               std::to_string(bmcLogId));
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(fmt::format("Failed to get BMC log id "
                                    "for the given EID (aka PEL ID) [{}]",
                                    eid)
                            .c_str());
        return std::nullopt;
    }
}

} // namespace hw_isolation
