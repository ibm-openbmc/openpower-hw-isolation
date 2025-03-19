// SPDX-License-Identifier: Apache-2.0

#include "config.h"

#include "hw_isolation_record/manager.hpp"

#include "common/common_types.hpp"
#include "common/utils.hpp"
#include "common/error_log.hpp"

#include <cereal/archives/binary.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/State/Chassis/server.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <ranges>
#include <sstream>

// Associate Manager Class with version number
constexpr uint32_t Cereal_ManagerClassVersion = 1;
CEREAL_CLASS_VERSION(hw_isolation::record::Manager, Cereal_ManagerClassVersion);

namespace hw_isolation
{
namespace record
{

using namespace phosphor::logging;
namespace fs = std::filesystem;

constexpr auto HW_ISOLATION_ENTRY_MGR_PERSIST_PATH =
    "/var/lib/op-hw-isolation/persistdata/record_mgr/{}";

Manager::Manager(sdbusplus::bus::bus& bus, const std::string& objPath,
                 const sdeventplus::Event& eventLoop) :
    type::ServerObject<CreateInterface, DeleteAllInterface>(bus,
                                                            objPath.c_str()),
    _bus(bus), _eventLoop(eventLoop), _isolatableHWs(bus),
    _guardFileWatch(
        eventLoop.get(), IN_NONBLOCK, IN_CLOSE_WRITE, EPOLLIN,
        openpower_guard::getGuardFilePath(),
        std::bind(std::mem_fn(&hw_isolation::record::Manager::
                                  processHardwareIsolationRecordFile),
                  this))
{
    fs::create_directories(
        fs::path(HW_ISOLATION_ENTRY_PERSIST_PATH).parent_path());

    deserialize();
}

void Manager::serialize()
{
    fs::path path{
        std::format(HW_ISOLATION_ENTRY_MGR_PERSIST_PATH, "eco_cores")};

    if (_persistedEcoCores.empty())
    {
        fs::remove(path);
        return;
    }

    fs::create_directories(path.parent_path());
    try
    {
        std::ofstream os(path.c_str(), std::ios::binary);
        cereal::BinaryOutputArchive oarchive(os);
        oarchive(*this);
    }
    catch (const cereal::Exception& e)
    {
        log<level::ERR>(std::format("Exception: [{}] during serialize the "
                                    "eco cores physical path into {}",
                                    e.what(), path.string())
                            .c_str());
        fs::remove(path);
    }
}

bool Manager::deserialize()
{
    fs::path path{
        std::format(HW_ISOLATION_ENTRY_MGR_PERSIST_PATH, "eco_cores")};
    try
    {
        if (fs::exists(path))
        {
            std::ifstream is(path.c_str(), std::ios::in | std::ios::binary);
            cereal::BinaryInputArchive iarchive(is);
            iarchive(*this);
            return true;
        }
        return false;
    }
    catch (const cereal::Exception& e)
    {
        log<level::ERR>(std::format("Exception: [{}] during deserialize the "
                                    "eco cores physical path into {}",
                                    e.what(), path.string())
                            .c_str());
        fs::remove(path);
        return false;
    }
}

void Manager::updateEcoCoresList(
    const bool ecoCore, const devtree::DevTreePhysPath& coreDevTreePhysPath)
{
    if (ecoCore)
    {
        _persistedEcoCores.emplace(coreDevTreePhysPath);
    }
    else
    {
        _persistedEcoCores.erase(coreDevTreePhysPath);
    }
    serialize();
}

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
        log<level::ERR>(std::format("Exception [{}] to get EID (aka PEL ID) "
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
        auto entryObjPath = fs::path(HW_ISOLATION_ENTRY_OBJPATH) /
                            std::to_string(recordId);

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

        _isolatedHardwares.insert(std::make_pair(
            recordId, std::make_unique<entry::Entry>(
                          _bus, entryObjPath, *this, recordId, severity,
                          resolved, associationDeftoHw, entityPath)));

        utils::setEnabledProperty(_bus, isolatedHardware, resolved);

        // Update the last entry id by using the created entry id.
        return entryObjPath.string();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(
            std::format("Exception [{}], so failed to create entry", e.what())
                .c_str());

        if (deleteRecord)
        {
            openpower_guard::clear(recordId);
        }
    }
    return std::nullopt;
}

std::pair<bool, sdbusplus::message::object_path> Manager::updateEntry(
    const entry::EntryRecordId& recordId, const entry::EntrySeverity& severity,
    const std::string& isolatedHwDbusObjPath, const std::string& bmcErrorLog,
    const openpower_guard::EntityPath& entityPath)
{
    auto isolatedHwIt =
        std::find_if(_isolatedHardwares.begin(), _isolatedHardwares.end(),
                     [recordId, entityPath](const auto& isolatedHw) {
        return ((isolatedHw.second->getEntityPath() == entityPath) &&
                (isolatedHw.second->getEntryRecId() == recordId));
    });

    if (isolatedHwIt == _isolatedHardwares.end())
    {
        // D-Bus entry does not exist
        return std::make_pair(false, std::string());
    }

    // Add association for isolated hardware inventory path
    // Note: Association forward and reverse type are defined as per
    // hardware isolation design document (aka guard) and hardware isolation
    // entry dbus interface document for hardware and error object path
    type::AsscDefFwdType isolateHwFwdType("isolated_hw");
    type::AsscDefRevType isolatedHwRevType("isolated_hw_entry");
    type::AssociationDef associationDeftoHw;
    associationDeftoHw.push_back(std::make_tuple(
        isolateHwFwdType, isolatedHwRevType, isolatedHwDbusObjPath));

    // Add errog log as Association if given
    if (!bmcErrorLog.empty())
    {
        type::AsscDefFwdType bmcErrorLogFwdType("isolated_hw_errorlog");
        type::AsscDefRevType bmcErrorLogRevType("isolated_hw_entry");
        associationDeftoHw.push_back(std::make_tuple(
            bmcErrorLogFwdType, bmcErrorLogRevType, bmcErrorLog));
    }

    // Existing record might be overridden in the libguard during
    // creation if that's meets certain override conditions
    bool updated{false};
    if (isolatedHwIt->second->severity() != severity)
    {
        isolatedHwIt->second->severity(severity);
        updated = true;
    }

    if (isolatedHwIt->second->associations() != associationDeftoHw)
    {
        isolatedHwIt->second->associations(associationDeftoHw);
        updated = true;
    }

    if (updated)
    {
        // Existing entry might be overwritten if that's meets certain
        // overwritten conditions so update creation time.
        std::time_t timeStamp = std::time(nullptr);
        isolatedHwIt->second->elapsed(timeStamp);
    }

    auto entryObjPath = fs::path(HW_ISOLATION_ENTRY_OBJPATH) /
                        std::to_string(isolatedHwIt->first);

    isolatedHwIt->second->serialize();
    return std::make_pair(true, entryObjPath.string());
}

void Manager::isHwIsolationAllowed(const entry::EntrySeverity& severity)
{
    // Make sure the hardware isolation setting is enabled or not
    if (!utils::isHwIosolationSettingEnabled(_bus))
    {
        log<level::INFO>(
            std::format("Hardware isolation is not allowed "
                        "since the HardwareIsolation setting is disabled")
                .c_str());
        throw type::CommonError::Unavailable();
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
            log<level::ERR>(std::format("Manual hardware isolation is allowed "
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
        log<level::ERR>(std::format("Invalid argument [IsolateHardware: {}]",
                                    isolateHardware.str)
                            .c_str());
        throw type::CommonError::InvalidArgument();
    }

    auto guardType = entry::utils::getGuardType(severity);
    if (!guardType.has_value())
    {
        log<level::ERR>(
            std::format("Invalid argument [Severity: {}]",
                        entry::EntryInterface::convertTypeToString(severity))
                .c_str());
        throw type::CommonError::InvalidArgument();
    }

    auto guardRecord = openpower_guard::create(
        openpower_guard::EntityPath(devTreePhysicalPath->data(),
                                    devTreePhysicalPath->size()),
        0, *guardType);

    if (auto ret = updateEntry(guardRecord->recordId, severity,
                               isolateHardware.str, "", guardRecord->targetId);
        ret.first == true)
    {
        return ret.second;
    }
    else
    {
        auto entryPath = createEntry(guardRecord->recordId, false, severity,
                                     isolateHardware.str, "", true,
                                     guardRecord->targetId);

        if (!entryPath.has_value())
        {
            throw type::CommonError::InternalFailure();
        }
        return *entryPath;
    }
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
        log<level::ERR>(std::format("Invalid argument [IsolateHardware: {}]",
                                    isolateHardware.str)
                            .c_str());
        throw type::CommonError::InvalidArgument();
    }

    auto eId = getEID(bmcErrorLog);
    if (!eId.has_value())
    {
        log<level::ERR>(
            std::format("Invalid argument [BmcErrorLog: {}]", bmcErrorLog.str)
                .c_str());
        throw type::CommonError::InvalidArgument();
    }

    auto guardType = entry::utils::getGuardType(severity);
    if (!guardType.has_value())
    {
        log<level::ERR>(
            std::format("Invalid argument [Severity: {}]",
                        entry::EntryInterface::convertTypeToString(severity))
                .c_str());
        throw type::CommonError::InvalidArgument();
    }

    auto guardRecord = openpower_guard::create(
        openpower_guard::EntityPath(devTreePhysicalPath->data(),
                                    devTreePhysicalPath->size()),
        *eId, *guardType);

    if (auto ret = updateEntry(guardRecord->recordId, severity,
                               isolateHardware.str, bmcErrorLog.str,
                               guardRecord->targetId);
        ret.first == true)
    {
        return ret.second;
    }
    else
    {
        auto entryPath = createEntry(guardRecord->recordId, false, severity,
                                     isolateHardware.str, bmcErrorLog.str, true,
                                     guardRecord->targetId);

        if (!entryPath.has_value())
        {
            throw type::CommonError::InternalFailure();
        }
        return *entryPath;
    }
}

void Manager::eraseEntry(const entry::EntryRecordId entryRecordId)
{
    if (_isolatedHardwares.contains(entryRecordId))
    {
        updateEcoCoresList(
            false, devtree::convertEntityPathIntoRawData(
                       _isolatedHardwares.at(entryRecordId)->getEntityPath()));
    }
    _isolatedHardwares.erase(entryRecordId);
}

void Manager::clearDbusEntries()
{
    auto entryIt = _isolatedHardwares.begin();
    while (entryIt != _isolatedHardwares.end())
    {
        auto entryRecordId = entryIt->first;
        auto& entry = entryIt->second;
        std::advance(entryIt, 1);

        // Continue other entries to delete if failed to delete one entry
        try
        {
            entry->resolveEntry(false);
        }
        catch (std::exception& e)
        {
            log<level::ERR>(std::format("Exception [{}] to delete entry [{}]",
                                        e.what(), entryRecordId)
                                .c_str());
        }
    }
}

void Manager::deleteAll()
{
    // When deleteall is invoked, the core records are not cleared.
    // So the gui should immediately reflect the records that are cleared.
    // Instead of waiting for guard file update, immediately refesh the dbus entries
    // with the number of records in the guard file
    hw_isolation::utils::isHwDeisolationAllowed(_bus);
    try
    {
        // remove the watch, as clearall is allowed only when the host is powered off
        // as we dont want inotify signals to come again
        _guardFileWatch.removeWatch();
        openpower_guard::clearAll();
        handleHostIsolatedHardwares();
        _guardFileWatch.addWatch();
    }
    //If there is any error in adding/ removing watch runtime_error will be generated
    //Create a PEL during that scenario  
    catch (const std::runtime_error& e) 
    {
        //create error pel and then throw the exception
        error_log::createErrorLog(error_log::HwIsolationGenericErrMsg,
            error_log::Level::Warning,
            error_log::CollectTraces);
        throw;
    }
    catch (...) 
    {
        log<level::ERR>("Unknown exception caught while ClearAll of dbus entries");
    }
}

bool Manager::isValidRecord(const entry::EntryRecordId recordId)
{
    if (recordId != 0xFFFFFFFF)
    {
        return true;
    }

    return false;
}

void Manager::createEntryForRecord(const openpower_guard::GuardRecord& record,
                                   const bool isRestorePath)
{
    auto entityPathRawData =
        devtree::convertEntityPathIntoRawData(record.targetId);
    std::stringstream ss;
    std::for_each(entityPathRawData.begin(), entityPathRawData.end(),
                  [&ss](const auto& ele) {
        ss << std::setw(2) << std::setfill('0') << std::hex << (int)ele << " ";
    });

    try
    {
        entry::EntryResolved resolved = false;
        if (record.recordId == 0xFFFFFFFF)
        {
            resolved = true;
        }

        bool ecoCore{
            (_persistedEcoCores.contains(entityPathRawData) && isRestorePath)};

        auto isolatedHwInventoryPath =
            _isolatableHWs.getInventoryPath(entityPathRawData, ecoCore);

        if (!isolatedHwInventoryPath.has_value())
        {
            log<level::ERR>(
                std::format(
                    "Skipping to restore a given isolated "
                    "hardware [{}] : Due to failure to get inventory path",
                    ss.str())
                    .c_str());
            return;
        }
        updateEcoCoresList(ecoCore, entityPathRawData);

        auto bmcErrorLogPath = utils::getBMCLogPath(_bus, record.elogId);
        std::string strBmcErrorLogPath{};
        strBmcErrorLogPath = bmcErrorLogPath->str;

        auto entrySeverity = entry::utils::getEntrySeverityType(
            static_cast<openpower_guard::GardType>(record.errType));
        if (!entrySeverity.has_value())
        {
            log<level::ERR>(
                std::format("Skipping to restore a given isolated "
                            "hardware [{}] : Due to failure to to get BMC "
                            "EntrySeverity by isolated hardware GardType [{}]",
                            ss.str(), record.errType)
                    .c_str());
            return;
        }

        auto entryPath = createEntry(record.recordId, resolved, *entrySeverity,
                                     isolatedHwInventoryPath->str,
                                     strBmcErrorLogPath, false,
                                     record.targetId);

        if (!entryPath.has_value())
        {
            log<level::ERR>(
                std::format(
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
            std::format("Exception [{}] : Skipping to restore a given isolated "
                        "hardware [{}]",
                        e.what(), ss.str())
                .c_str());
    }
}

void Manager::updateEntryForRecord(const openpower_guard::GuardRecord& record,
                                   IsolatedHardwares::iterator& entryIt)
{
    auto entityPathRawData =
        devtree::convertEntityPathIntoRawData(record.targetId);
    std::stringstream ss;
    std::for_each(entityPathRawData.begin(), entityPathRawData.end(),
                  [&ss](const auto& ele) {
        ss << std::setw(2) << std::setfill('0') << std::hex << (int)ele << " ";
    });

    bool ecoCore{false};

    auto isolatedHwInventoryPath =
        _isolatableHWs.getInventoryPath(entityPathRawData, ecoCore);

    if (!isolatedHwInventoryPath.has_value())
    {
        log<level::ERR>(
            std::format("Skipping to restore a given isolated "
                        "hardware [{}] : Due to failure to get inventory path",
                        ss.str())
                .c_str());
        return;
    }
    updateEcoCoresList(ecoCore, entityPathRawData);

    auto bmcErrorLogPath = utils::getBMCLogPath(_bus, record.elogId);

    auto entrySeverity = entry::utils::getEntrySeverityType(
        static_cast<openpower_guard::GardType>(record.errType));
    if (!entrySeverity.has_value())
    {
        log<level::ERR>(
            std::format("Skipping to restore a given isolated "
                        "hardware [{}] : Due to failure to to get BMC "
                        "EntrySeverity by isolated hardware GardType [{}]",
                        ss.str(), record.errType)
                .c_str());
        return;
    }

    // Add association for isolated hardware inventory path
    // Note: Association forward and reverse type are defined as per
    // hardware isolation design document (aka guard) and hardware isolation
    // entry dbus interface document for hardware and error object path
    type::AsscDefFwdType isolateHwFwdType("isolated_hw");
    type::AsscDefRevType isolatedHwRevType("isolated_hw_entry");
    type::AssociationDef associationDeftoHw;
    associationDeftoHw.push_back(std::make_tuple(
        isolateHwFwdType, isolatedHwRevType, *isolatedHwInventoryPath));

    // Add errog log as Association if given
    if (!bmcErrorLogPath->str.empty())
    {
        type::AsscDefFwdType bmcErrorLogFwdType("isolated_hw_errorlog");
        type::AsscDefRevType bmcErrorLogRevType("isolated_hw_entry");
        associationDeftoHw.push_back(std::make_tuple(
            bmcErrorLogFwdType, bmcErrorLogRevType, *bmcErrorLogPath));
    }

    bool updated{false};
    if (entryIt->second->severity() != entrySeverity)
    {
        entryIt->second->severity(*entrySeverity);
        updated = true;
    }

    if (entryIt->second->associations() != associationDeftoHw)
    {
        entryIt->second->associations(associationDeftoHw);
        updated = true;
    }

    utils::setEnabledProperty(_bus, *isolatedHwInventoryPath, false);

    if (updated)
    {
        // Existing entry might be overwritten if that's meets certain
        // overwritten conditions so update creation time.
        std::time_t timeStamp = std::time(nullptr);
        entryIt->second->elapsed(timeStamp);
    }

    entryIt->second->serialize();
}

void Manager::cleanupPersistedEcoCores()
{
    bool updated{false};
    if (_isolatedHardwares.empty())
    {
        _persistedEcoCores.clear();
        updated = true;
    }
    else
    {
        for (auto ecoCore = _persistedEcoCores.begin();
             ecoCore != _persistedEcoCores.end();)
        {
            auto nextEcoCore = std::next(ecoCore, 1);

            auto isNotIsolated = std::ranges::none_of(
                _isolatedHardwares, [ecoCore](const auto& entry) {
                return (entry.second->getEntityPath() ==
                        openpower_guard::EntityPath(ecoCore->data(),
                                                    ecoCore->size()));
            });

            if (isNotIsolated)
            {
                updateEcoCoresList(false, *ecoCore);
                updated = true;
            }

            ecoCore = nextEcoCore;
        }
    }

    if (updated)
    {
        serialize();
    }
}

void Manager::cleanupPersistedFiles()
{
    auto deletePersistedEntryFileIfNotExist = [this](const auto& file) {
        auto fileEntryId = std::stoul(file.path().filename());

        if (!(this->_isolatedHardwares.contains(fileEntryId)))
        {
            fs::remove(file.path());
        }
    };

    std::ranges::for_each(
        fs::directory_iterator(
            fs::path(HW_ISOLATION_ENTRY_PERSIST_PATH).parent_path()),
        deletePersistedEntryFileIfNotExist);

    cleanupPersistedEcoCores();
}

void Manager::restore()
{
    // Don't get ephemeral records (GARD_Reconfig and GARD_Sticky_deconfig
    // because those type records are created for internal purpose to use
    // by BMC and Hostboot
    openpower_guard::GuardRecords records = openpower_guard::getAll(true);

    auto validRecord = [this](const auto& record) {
        return this->isValidRecord(record.recordId);
    };

    auto validRecords = records | std::views::filter(validRecord);

    auto createEntry = [this](const auto& record) {
        this->createEntryForRecord(record, true);
    };

    std::ranges::for_each(validRecords, createEntry);

    cleanupPersistedFiles();
}

void Manager::processHardwareIsolationRecordFile()
{
    /**
     * Start timer in the event loop to get the final isolated hardware
     * record list which are updated by the host because of the atomicity
     * on the partition file (which is used to store isolated hardware details)
     * between BMC and Host.
     */
    try
    {
        // The handleHostIsolatedHardwares method is called after 5 seconds to
        // handle atomicity in the guard file operations. Within this time
        // window, if there are multiple updates to the guard file, process all
        // of them together. We need not add another timer object to process the
        // new information, as the earlier information is not yet processed and
        // could be done together. In every iteration we process and update all
        // the guard records. This will optimize the time consumed when there
        // are multiple guard records created.
        if (_timerObjs.empty())
        {
            _timerObjs.emplace(std::make_unique<sdeventplus::utility::Timer<
                                   sdeventplus::ClockId::Monotonic>>(
                _eventLoop,
                std::bind(std::mem_fn(&hw_isolation::record::Manager::
                                          handleHostIsolatedHardwares),
                          this),
                std::chrono::seconds(5)));
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(
            std::format("Exception [{}], Failed to process "
                        "hardware isolation record file that's updated",
                        e.what())
                .c_str());
    }
}

void Manager::handleHostIsolatedHardwares()
{
    if (!_timerObjs.empty())
    {
        auto timerObj = std::move(_timerObjs.front());
        _timerObjs.pop();
        if (timerObj->isEnabled())
        {
            timerObj->setEnabled(false);
        }
    }

    // Don't get ephemeral records (GARD_Reconfig and GARD_Sticky_deconfig
    // because those type records are created for internal purpose to use
    // by BMC and Hostboot
    openpower_guard::GuardRecords records = openpower_guard::getAll(true);

    // Delete all the D-Bus entries if no record in their persisted location
    if ((records.size() == 0) && _isolatedHardwares.size() > 0)
    {
        // Clean up all entries association before delete.
        clearDbusEntries();
        _isolatedHardwares.clear();
        return;
    }

    auto validRecord = [this](const auto& record) {
        return this->isValidRecord(record.recordId);
    };

    for (auto entryIt = _isolatedHardwares.begin();
         entryIt != _isolatedHardwares.end();)
    {
        auto nextEntryIt = std::next(entryIt, 1);

        auto entryRecord = [entryIt](const auto& record) {
            return entryIt->second->getEntityPath() == record.targetId;
        };
        auto entryRecords = records | std::views::filter(entryRecord);

        if (entryRecords.empty())
        {
            entryIt->second->resolveEntry(false);
        }
        else
        {
            auto validEntryRecords = entryRecords |
                                     std::views::filter(validRecord);

            if (validEntryRecords.empty())
            {
                entryIt->second->resolveEntry(false);
            }
            else if (std::distance(validEntryRecords.begin(),
                                   validEntryRecords.end()) == 1)
            {
                this->updateEntryForRecord(validEntryRecords.front(), entryIt);
            }
            else
            {
                // Should not happen since, more than one valid records
                // for the same hardware is not allowed
                auto entityPathRawData = devtree::convertEntityPathIntoRawData(
                    entryIt->second->getEntityPath());
                std::stringstream ss;
                std::for_each(entityPathRawData.begin(),
                              entityPathRawData.end(), [&ss](const auto& ele) {
                    ss << std::setw(2) << std::setfill('0') << std::hex
                       << (int)ele << " ";
                });
                log<level::ERR>(std::format("More than one valid records exist "
                                            "for the same hardware [{}]",
                                            ss.str())
                                    .c_str());
            }
        }
        entryIt = nextEntryIt;
    }

    auto validRecords = records | std::views::filter(validRecord);

    auto createEntryIfNotExists = [this](const auto& validRecord) {
        auto recordExist = [validRecord](const auto& entry) {
            return validRecord.targetId == entry.second->getEntityPath();
        };

        if (std::ranges::none_of(this->_isolatedHardwares, recordExist))
        {
            this->createEntryForRecord(validRecord);
        }
    };

    std::ranges::for_each(validRecords, createEntryIfNotExists);

    cleanupPersistedEcoCores();
}

std::optional<std::tuple<entry::EntrySeverity, entry::EntryErrLogPath>>
    Manager::getIsolatedHwRecordInfo(
        const sdbusplus::message::object_path& hwInventoryPath)
{
    // If there is more than one hw isolation entry matching the inventory
    // The possibility of that is very less as we do not intend to create
    // more than 1 record per physical dimm.
    std::vector<hw_isolation::record::IsolatedHardwares::iterator>
        entriesIterators;

    // Make sure whether the given hardware inventory is exists
    // in the record list.
    for (auto it = _isolatedHardwares.begin(); it != _isolatedHardwares.end();
         ++it)
    {
        if ([&it, hwInventoryPath]() {
            for (const auto& assocEle : it->second->associations())
            {
                if (std::get<0>(assocEle) == "isolated_hw")
                {
                    return std::get<2>(assocEle) == hwInventoryPath.str;
                }
            }
            return false;
        }())
        {
            // Get all the HW Isolation entries that match the inventory path
            // For Dimms, there could be more than one entry
            entriesIterators.push_back(it);
        }
    }
    // inventory path  not found
    if (entriesIterators.size() == 0)
    {
        return std::nullopt;
    }

    std::vector<entry::EntryErrLogPath> errLogPathList;
    std::vector<entry::EntrySeverity> severityList;

    // Check which has highest priority and use it
    for (auto entryIt : entriesIterators)
    {
        for (const auto& assocEle : entryIt->second->associations())
        {
            if (std::get<0>(assocEle) == "isolated_hw_errorlog")
            {
                errLogPathList.push_back(std::get<2>(assocEle));
                break;
            }
        }
        severityList.push_back(entryIt->second->severity());
        // No error log found for it. So push empty string
        if (errLogPathList.size() != severityList.size())
        {
            errLogPathList.push_back("");
        }
    }

    // Now based on the priority get the Severity that needs to be used.
    int index = getHigherPrecendenceEntry(severityList);
    return std::make_tuple(severityList[index], errLogPathList[index]);
}

int Manager::getHigherPrecendenceEntry(
    std::vector<entry::EntrySeverity>& entrySeverityList)
{
    // Check if the eventMsgList has only one element
    if (entrySeverityList.size() == 1)
    {
        // If there's only one element, return 0 the index of first element
        return 0;
    }
    else
    {
        /*The different deconfig types that are allowed
        "Fatal", event::EntrySeverity::Critical
        "Manual", event::EntrySeverity::Ok
        "Predictive", event::EntrySeverity::Warning
        "Unknown", event::EntrySeverity::Warning */

        // Define a vector containing Deconfiguration Type in the following
        // precedence
        std::vector<entry::EntrySeverity> deconfigTypes = {
            entry::EntrySeverity::Spare, entry::EntrySeverity::Critical,
            entry::EntrySeverity::Warning, entry::EntrySeverity::Manual};

        // Iterate through each keyword
        for (const auto& deconfigType : deconfigTypes)
        {
            // Iterate through each element in the eventMsgList
            for (unsigned int i = 0; i < entrySeverityList.size(); ++i)
            {
                // Check if the current element is of higher precedence
                if (entrySeverityList[i] == deconfigType)
                {
                    // If found, return the index
                    return i;
                }
            }
        }
    }
    // If none of the conditions are met, return 0 to use first index
    return 0;
}

} // namespace record
} // namespace hw_isolation
