// SPDX-License-Identifier: Apache-2.0

#include "config.h"

#include "hw_isolation_record/manager.hpp"

#include "common/common_types.hpp"
#include "common/utils.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/State/Chassis/server.hpp>

#include <filesystem>
#include <iomanip>
#include <sstream>

namespace hw_isolation
{
namespace record
{

using namespace phosphor::logging;
namespace fs = std::filesystem;

Manager::Manager(sdbusplus::bus::bus& bus, const std::string& objPath,
                 const sd_event* eventLoop) :
    type::ServerObject<CreateInterface, OP_CreateInterface, DeleteAllInterface>(
        bus, objPath.c_str(), true),
    _bus(bus), _lastEntryId(0), _isolatableHWs(bus),
    _guardFileWatch(
        eventLoop, IN_NONBLOCK, IN_CLOSE_WRITE, EPOLLIN,
        openpower_guard::getGuardFilePath(),
        std::bind(
            std::mem_fn(
                &hw_isolation::record::Manager::handleHostIsolatedHardwares),
            this))
{}

std::optional<uint32_t>
    Manager::getEID(const sdbusplus::message::object_path& bmcErrorLog) const
{
    try
    {
        uint32_t eid;

        auto dbusServiceName = utils::getDBusServiceName(
            _bus, type::LoggingObjectPath, type::LoggingInterface);

        auto method = _bus.new_method_call(
            dbusServiceName.c_str(), type::LoggingObjectPath,
            type::LoggingInterface, "GetPELIdFromBMCLogId");

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

std::optional<sdbusplus::message::object_path> Manager::createEntry(
    const entry::EntryRecordId& recordId, const entry::EntryResolved& resolved,
    const entry::EntrySeverity& severity, const std::string& isolatedHardware,
    const std::string& bmcErrorLog, const bool deleteRecord,
    const openpower_guard::EntityPath& entityPath)
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
        type::AsscDefFwdType isolateHwFwdType("isolated_hw");
        type::AsscDefRevType isolatedHwRevType("isolated_hw_entry");
        type::AssociationDef associationDeftoHw;
        associationDeftoHw.push_back(std::make_tuple(
            isolateHwFwdType, isolatedHwRevType, isolatedHardware));

        // Add errog log as Association if given
        if (!bmcErrorLog.empty())
        {
            type::AsscDefFwdType bmcErrorLogFwdType("isolated_hw_errorlog");
            type::AsscDefRevType bmcErrorLogRevType("isolated_hw_entry");
            associationDeftoHw.push_back(std::make_tuple(
                bmcErrorLogFwdType, bmcErrorLogRevType, bmcErrorLog));
        }

        _isolatedHardwares.insert(
            std::make_pair(id, std::make_unique<entry::Entry>(
                                   _bus, entryObjPath, id, recordId, severity,
                                   resolved, associationDeftoHw, entityPath)));

        utils::setEnabledProperty(_bus, isolatedHardware, false);

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
    // Make sure the hardware isolation setting is enabled or not
    if (!utils::isHwIosolationSettingEnabled(_bus))
    {
        log<level::INFO>(
            fmt::format("Hardware isolation is not allowed "
                        "since the HardwareIsolation setting is disabled")
                .c_str());
        throw type::CommonError::NotAllowed();
    }

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

    auto guardType = entry::utils::getGuardType(severity);
    if (!guardType.has_value())
    {
        log<level::ERR>(
            fmt::format("Invalid argument [Severity: {}]",
                        entry::EntryInterface::convertTypeToString(severity))
                .c_str());
        throw type::CommonError::InvalidArgument();
    }

    auto guardRecord =
        openpower_guard::create(devTreePhysicalPath->data(), 0, *guardType);

    auto entryPath =
        createEntry(guardRecord->recordId, false, severity, isolateHardware.str,
                    "", true, guardRecord->targetId);

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

    auto guardType = entry::utils::getGuardType(severity);
    if (!guardType.has_value())
    {
        log<level::ERR>(
            fmt::format("Invalid argument [Severity: {}]",
                        entry::EntryInterface::convertTypeToString(severity))
                .c_str());
        throw type::CommonError::InvalidArgument();
    }

    auto guardRecord =
        openpower_guard::create(devTreePhysicalPath->data(), *eId, *guardType);

    auto entryPath =
        createEntry(guardRecord->recordId, false, severity, isolateHardware.str,
                    bmcErrorLog.str, true, guardRecord->targetId);

    if (!entryPath.has_value())
    {
        throw type::CommonError::InternalFailure();
    }
    return *entryPath;
}

void Manager::deleteAll()
{
    if (!hw_isolation::utils::isHwDeisolationAllowed(_bus))
    {
        throw type::CommonError::NotAllowed();
    }

    auto entryIt = _isolatedHardwares.begin();
    while (entryIt != _isolatedHardwares.end())
    {
        auto entryId = entryIt->first;
        auto& entry = entryIt->second;
        std::advance(entryIt, 1);

        // Continue other entries to delete if failed to delete one entry
        try
        {
            entry->delete_();
        }
        catch (std::exception& e)
        {
            log<level::ERR>(fmt::format("Exception [{}] to delete entry [{}]",
                                        e.what(), entryId)
                                .c_str());
        }
    }
}

void Manager::createEntryForRecord(const openpower_guard::GuardRecord& record)
{
    auto entityPathRawData =
        devtree::convertEntityPathIntoRawData(record.targetId);
    std::stringstream ss;
    std::for_each(entityPathRawData.begin(), entityPathRawData.end(),
                  [&ss](const auto& ele) {
                      ss << std::setw(2) << std::setfill('0') << std::hex
                         << (int)ele << " ";
                  });

    try
    {
        entry::EntryResolved resolved = false;
        if (record.recordId == 0xFFFFFFFF)
        {
            resolved = true;
        }

        auto isolatedHwInventoryPath =
            _isolatableHWs.getInventoryPath(entityPathRawData);

        if (!isolatedHwInventoryPath.has_value())
        {
            log<level::ERR>(
                fmt::format(
                    "Skipping to restore a given isolated "
                    "hardware [{}] : Due to failure to get inventory path",
                    ss.str())
                    .c_str());
            return;
        }

        auto bmcErrorLogPath = utils::getBMCLogPath(_bus, record.elogId);

        if (!bmcErrorLogPath.has_value())
        {
            log<level::ERR>(
                fmt::format(
                    "Skipping to restore a given isolated "
                    "hardware [{}] : Due to failure to get BMC error log path "
                    "by isolated hardware EID (aka PEL ID) [{}]",
                    ss.str(), record.elogId)
                    .c_str());
            return;
        }

        auto entrySeverity = entry::utils::getEntrySeverityType(
            static_cast<openpower_guard::GardType>(record.errType));
        if (!entrySeverity.has_value())
        {
            log<level::ERR>(
                fmt::format("Skipping to restore a given isolated "
                            "hardware [{}] : Due to failure to to get BMC "
                            "EntrySeverity by isolated hardware GardType [{}]",
                            ss.str(), record.errType)
                    .c_str());
            return;
        }

        auto entryPath =
            createEntry(record.recordId, resolved, *entrySeverity,
                        isolatedHwInventoryPath->str, bmcErrorLogPath->str,
                        false, record.targetId);

        if (!entryPath.has_value())
        {
            log<level::ERR>(
                fmt::format(
                    "Skipping to restore a given isolated "
                    "hardware [{}] : Due to failure to create dbus entry",
                    ss.str())
                    .c_str());
            return;
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(
            fmt::format("Exception [{}] : Skipping to restore a given isolated "
                        "hardware [{}]",
                        e.what(), ss.str())
                .c_str());
    }
}

void Manager::restore()
{
    openpower_guard::GuardRecords records = openpower_guard::getAll();

    std::for_each(records.begin(), records.end(), [this](const auto& record) {
        // Skipping fake record (GARD_Reconfig) because,
        // fake record is created for internal purpose to
        // to use by BMC and Hostboot
        if (record.errType == openpower_guard::GardType::GARD_Reconfig)
        {
            return;
        }

        this->createEntryForRecord(record);
    });
}

void Manager::handleHostIsolatedHardwares()
{
    // Wait for some time to get the final isolated hardware record list
    // which are updated by the host because of the atomicity on partition
    // file (which is used to isolated hardware details) between BMC and Host.
    sleep(5);

    openpower_guard::GuardRecords records = openpower_guard::getAll();

    // Delete all the D-Bus entries if no record in their persisted location
    if ((records.size() == 0) && _isolatedHardwares.size() > 0)
    {
        _isolatedHardwares.clear();
        _lastEntryId = 0;
        return;
    }

    std::for_each(records.begin(), records.end(), [this](const auto& record) {
        // Skipping fake record (GARD_Reconfig) because, fake record is
        // created for internal purposes to use by BMC and Hostboot.
        if (record.errType == openpower_guard::GardType::GARD_Reconfig)
        {
            return;
        }

        // Make sure the isolated hardware record is resolved (recordId will
        // get change as "0xFFFFFFFF") by host OR the record is already exist.
        auto isolatedHwIt = std::find_if(
            _isolatedHardwares.begin(), _isolatedHardwares.end(),
            [&record](const auto& isolatedHw) {
                return (
                    (isolatedHw.second->getEntityPath() == record.targetId) &&
                    ((record.recordId == 0xFFFFFFFF) ||
                     (isolatedHw.second->getEntryRecId() == record.recordId)));
            });

        // If not found in the existing isolated hardware entries list then
        // the host created a new record for isolating some hardware so
        // add the new dbus entry for the same.
        if (isolatedHwIt == _isolatedHardwares.end())
        {
            this->createEntryForRecord(record);
        }
        // Update Resolved property if a record is resolved otherwise skip
        // the record since it is already exist in isolated hardware entries.
        else if (record.recordId == 0xFFFFFFFF)
        {
            isolatedHwIt->second->resolved(true);
        }
    });
}

sdbusplus::message::object_path Manager::createWithEntityPath(
    std::vector<uint8_t> entityPath,
    sdbusplus::xyz::openbmc_project::HardwareIsolation::server::Entry::Type
        severity,
    sdbusplus::message::object_path bmcErrorLog)
{
    isHwIsolationAllowed(severity);

    auto isolateHwInventoryPath = _isolatableHWs.getInventoryPath(entityPath);
    std::stringstream ss;
    std::for_each(entityPath.begin(), entityPath.end(), [&ss](const auto& ele) {
        ss << std::setw(2) << std::setfill('0') << std::hex << (int)ele << " ";
    });
    if (!isolateHwInventoryPath.has_value())
    {
        log<level::ERR>(
            fmt::format("Invalid argument [IsolateHardware: {}]", ss.str())
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

    auto guardType = entry::utils::getGuardType(severity);
    if (!guardType.has_value())
    {
        log<level::ERR>(
            fmt::format("Invalid argument [Severity: {}]",
                        entry::EntryInterface::convertTypeToString(severity))
                .c_str());
        throw type::CommonError::InvalidArgument();
    }

    auto guardRecord =
        openpower_guard::create(entityPath.data(), *eId, *guardType);

    auto entryPath = createEntry(guardRecord->recordId, false, severity,
                                 isolateHwInventoryPath->str, bmcErrorLog.str,
                                 true, guardRecord->targetId);

    if (!entryPath.has_value())
    {
        throw type::CommonError::InternalFailure();
    }
    return *entryPath;
}

} // namespace record
} // namespace hw_isolation
