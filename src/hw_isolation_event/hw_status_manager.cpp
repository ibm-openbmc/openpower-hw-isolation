// SPDX-License-Identifier: Apache-2.0

extern "C"
{
#include <libpdbg.h>
}

#include "config.h"

#include "attributes_info.H"

#include "common/error_log.hpp"
#include "common/utils.hpp"
#include "hw_isolation_event/hw_status_manager.hpp"
#include "hw_isolation_event/openpower_hw_status.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>

#include <filesystem>

namespace hw_isolation
{
namespace event
{
namespace hw_status
{
namespace dbus_type
{
using Interface = std::string;
using Property = std::string;
using PropertyValue = std::variant<std::string, bool>;
using Properties = std::map<Property, PropertyValue>;
using Interfaces = std::map<Interface, Properties>;
} // namespace dbus_type

using namespace phosphor::logging;
namespace fs = std::filesystem;

constexpr auto HW_STATUS_EVENTS_PATH =
    HW_ISOLATION_OBJPATH "/events/hw_isolation_status";

constexpr auto HOST_STATE_OBJ_PATH = "/xyz/openbmc_project/state/host0";

Manager::Manager(sdbusplus::bus::bus& bus, const sdeventplus::Event& eventLoop,
                 record::Manager& hwIsolationRecordMgr) :
    _bus(bus),
    _eventLoop(eventLoop), _lastEventId(0), _isolatableHWs(bus),
    _hwIsolationRecordMgr(hwIsolationRecordMgr),
    _requiredHwsPdbgClass({"dimm", "fc"})
{
    fs::create_directories(
        fs::path(HW_ISOLATION_EVENT_PERSIST_PATH).parent_path());

    // Adding the required D-Bus match rules to create hardware status event
    // if interested signal is occurred.
    try
    {
        namespace sdbusplus_match = sdbusplus::bus::match;

        // Watch xyz.openbmc_project.State.Host::CurrentHostState property
        // change to take appropriate action for the hardware status event
        _dbusSignalWatcher.push_back(std::make_unique<sdbusplus_match::match>(
            _bus,
            sdbusplus_match::rules::propertiesChanged(
                HOST_STATE_OBJ_PATH, "xyz.openbmc_project.State.Host"),
            std::bind(std::mem_fn(&Manager::onHostStateChange), this,
                      std::placeholders::_1)));

        // Watch xyz.openbmc_project.State.Boot.Progress::BootProgress property
        // change to take appropriate action for the hardware status event
        _dbusSignalWatcher.push_back(std::make_unique<sdbusplus_match::match>(
            _bus,
            sdbusplus_match::rules::propertiesChanged(
                HOST_STATE_OBJ_PATH, "xyz.openbmc_project.State.Boot.Progress"),
            std::bind(std::mem_fn(&Manager::onBootProgressChange), this,
                      std::placeholders::_1)));
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(
            fmt::format("Exception [{}] while adding the D-Bus match rules",
                        e.what())
                .c_str());
        error_log::createErrorLog(error_log::HwIsolationGenericErrMsg,
                                  error_log::Level::Informational,
                                  error_log::CollectTraces);
    }
}

std::optional<sdbusplus::message::object_path> Manager::createEvent(
    const EventSeverity& eventSeverity, const EventMsg& eventMsg,
    const std::string& hwInventoryPath, const std::string& bmcErrorLogPath)
{
    try
    {
        auto id = _lastEventId + 1;
        auto eventObjPath =
            fs::path(HW_STATUS_EVENTS_PATH) / std::to_string(id);

        // Add association for the hareware inventory path which needs
        // the hardware status event.
        // Note: Association forward and reverse type are defined as per
        // xyz::openbmc_project::Logging::Event interface associations
        // documentation.
        type::AsscDefFwdType eventIndicatorFwdType{"event_indicator"};
        type::AsscDefRevType eventIndicatorRevType{"event_log"};
        type::AssociationDef associationDeftoEvent;
        associationDeftoEvent.push_back(std::make_tuple(
            eventIndicatorFwdType, eventIndicatorRevType, hwInventoryPath));

        // Add the error_log if given
        if (!bmcErrorLogPath.empty())
        {
            type::AsscDefFwdType errorLogFwdType{"error_log"};
            type::AsscDefFwdType errorLogRevType{"event_log"};
            associationDeftoEvent.push_back(std::make_tuple(
                errorLogFwdType, errorLogRevType, bmcErrorLogPath));
        }

        _hwStatusEvents.insert(
            std::make_pair(id, std::make_unique<hw_isolation::event::Event>(
                                   _bus, eventObjPath, id, eventSeverity,
                                   eventMsg, associationDeftoEvent)));

        // Update the last event id using the created event id;
        _lastEventId = id;
        return eventObjPath.string();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(
            fmt::format("Exception [{}], so failed to create hardware "
                        "status event",
                        e.what())
                .c_str());
    }
    return std::nullopt;
}

void Manager::clearHardwaresStatusEvent()
{
    // Remove all the existing hardware status event and
    // reset the last event id as "0"
    _hwStatusEvents.clear();
    _lastEventId = 0;
}

std::pair<event::EventMsg, event::EventSeverity>
    Manager::getIsolatedHwStatusInfo(
        const record::entry::EntrySeverity& recSeverity)
{
    switch (recSeverity)
    {
        case record::entry::EntrySeverity::Critical:
            return std::make_pair("Fatal", event::EventSeverity::Critical);
        case record::entry::EntrySeverity::Warning:
            return std::make_pair("Predictive", event::EventSeverity::Warning);
        case record::entry::EntrySeverity::Manual:
            return std::make_pair("Manual", event::EventSeverity::Ok);
        default:
            log<level::ERR>(
                fmt::format(
                    "Unsupported hardware isolation entry severity [{}]",
                    record::entry::EntryInterface::convertTypeToString(
                        recSeverity))
                    .c_str());
            error_log::createErrorLog(error_log::HwIsolationGenericErrMsg,
                                      error_log::Level::Informational,
                                      error_log::CollectTraces);
            return std::make_pair("Unknown", event::EventSeverity::Warning);
    }
}

void Manager::restoreHardwaresStatusEvent(bool osRunning)
{
    clearHardwaresStatusEvent();

    std::for_each(
        _requiredHwsPdbgClass.begin(), _requiredHwsPdbgClass.end(),
        [this, osRunning](const auto& ele) {
            struct pdbg_target* tgt;
            pdbg_for_each_class_target(ele.c_str(), tgt)
            {
                try
                {
                    if (ele == "fc")
                    {
                        struct pdbg_target* coreTgt;
                        bool ecoCore{false};
                        pdbg_for_each_target("core", tgt, coreTgt)
                        {
                            if (devtree::isECOcore(coreTgt))
                            {
                                ecoCore = true;
                                break;
                            }
                        }
                        if (ecoCore)
                        {
                            // ECO core is not modelled in the inventory so,
                            // event is not required to display the state of
                            // the core.
                            continue;
                        }
                    }

                    ATTR_HWAS_STATE_Type hwasState;
                    if (DT_GET_PROP(ATTR_HWAS_STATE, tgt, hwasState))
                    {
                        log<level::ERR>(
                            fmt::format("Skipping to create the hardware "
                                        "status event because failed to get "
                                        "ATTR_HWAS_STATE from [{}]",
                                        pdbg_target_path(tgt))
                                .c_str());
                        error_log::createErrorLog(
                            error_log::HwIsolationGenericErrMsg,
                            error_log::Level::Informational,
                            error_log::CollectTraces);
                        continue;
                    }

                    if (hwasState.present)
                    {
                        ATTR_PHYS_BIN_PATH_Type physBinPath;
                        if (DT_GET_PROP(ATTR_PHYS_BIN_PATH, tgt, physBinPath))
                        {
                            log<level::ERR>(
                                fmt::format(
                                    "Skipping to create the hardware "
                                    "status event because failed to get "
                                    "ATTR_PHYS_BIN_PATH from [{}]",
                                    pdbg_target_path(tgt))
                                    .c_str());
                            error_log::createErrorLog(
                                error_log::HwIsolationGenericErrMsg,
                                error_log::Level::Informational,
                                error_log::CollectTraces);
                            continue;
                        }

                        devtree::DevTreePhysPath devTreePhysPath;
                        std::copy(std::begin(physBinPath),
                                  std::end(physBinPath),
                                  std::back_inserter(devTreePhysPath));

                        // TODO: It is a workaround until fix the following
                        //       issue ibm-openbmc/dev/issues/3573.
                        bool ecoCore{false};
                        auto hwInventoryPath = _isolatableHWs.getInventoryPath(
                            devTreePhysPath, ecoCore);

                        if (!hwInventoryPath.has_value())
                        {
                            log<level::ERR>(
                                fmt::format(
                                    "Skipping to create the hardware "
                                    "status event because unable to find "
                                    "the inventory path for the given "
                                    "hardware [{}]",
                                    pdbg_target_path(tgt))
                                    .c_str());
                            error_log::createErrorLog(
                                error_log::HwIsolationGenericErrMsg,
                                error_log::Level::Informational,
                                error_log::CollectTraces);
                            continue;
                        }

                        event::EventMsg eventMsg;
                        event::EventSeverity eventSeverity;
                        record::entry::EntryErrLogPath eventErrLogPath;

                        auto isolatedhwRecordInfo =
                            _hwIsolationRecordMgr.getIsolatedHwRecordInfo(
                                *hwInventoryPath);

                        if (isolatedhwRecordInfo.has_value())
                        {
                            if (hwasState.functional)
                            {
                                auto functionalInInventory =
                                    utils::getDBusPropertyVal<bool>(
                                        _bus, hwInventoryPath->str,
                                        "xyz.openbmc_project.State.Decorator."
                                        "OperationalStatus",
                                        "Functional");

                                if (functionalInInventory &&
                                    (hwasState.deconfiguredByEid ==
                                     openpower_hw_status::DeconfiguredByReason::
                                         CONFIGURED_BY_RESOURCE_RECOVERY))
                                {
                                    /**
                                     * Event is required since the hardware is
                                     * recovered even thats requested to
                                     * isolate.
                                     */
                                    auto dfgReason = openpower_hw_status::
                                        convertDeconfiguredByReasonFromEnum(
                                            static_cast<
                                                openpower_hw_status::
                                                    DeconfiguredByReason>(
                                                hwasState.deconfiguredByEid));
                                    eventMsg = std::get<0>(dfgReason);
                                    eventSeverity = std::get<1>(dfgReason);
                                }
                                else if (!functionalInInventory && osRunning)
                                {
                                    /**
                                     * Event is required since the hardware is
                                     * deallocated during OS running.
                                     *
                                     * Assumption is, HWAS_STATE won't updated
                                     * for the runtime deallocation.
                                     */
                                    eventErrLogPath =
                                        std::get<1>(*isolatedhwRecordInfo);

                                    auto hwStatusInfo = getIsolatedHwStatusInfo(
                                        std::get<0>(*isolatedhwRecordInfo));

                                    eventMsg = std::get<0>(hwStatusInfo);
                                    eventSeverity = std::get<1>(hwStatusInfo);
                                }
                                else
                                {
                                    /**
                                     * Event is not required since the hardware
                                     * isolation record is exist and not applied
                                     * so far.
                                     */
                                    continue;
                                }
                            }
                            else
                            {
                                // Error log might be present or not in the
                                // record.
                                eventErrLogPath =
                                    std::get<1>(*isolatedhwRecordInfo);

                                auto hwStatusInfo = getIsolatedHwStatusInfo(
                                    std::get<0>(*isolatedhwRecordInfo));

                                eventMsg = std::get<0>(hwStatusInfo);
                                eventSeverity = std::get<1>(hwStatusInfo);
                            }
                        }
                        else
                        {
                            /**
                             * Update the "Enabled" property of the hardware
                             * because, we should allow to manually deconfigure
                             * a hardware with the hw-isolation record.
                             */
                            hw_isolation::utils::setEnabledProperty(
                                _bus, hwInventoryPath->str, true);

                            if (hwasState.functional)
                            {
                                // Event is not required since it is functional
                                continue;
                            }

                            if ((hwasState.deconfiguredByEid &
                                 openpower_hw_status::DeconfiguredByReason::
                                     DECONFIGURED_BY_PLID_MASK) != 0)
                            {
                                /**
                                 * Event is required since the hardware is
                                 * temporarily isolated by the error.
                                 */
                                auto eId = hwasState.deconfiguredByEid;
                                eventMsg = "Error";
                                eventSeverity = event::EventSeverity::Critical;

                                auto logObjPath =
                                    utils::getBMCLogPath(_bus, eId);
                                if (!logObjPath.has_value())
                                {
                                    log<level::ERR>(
                                        fmt::format(
                                            "Skipping to create the hardware "
                                            "status event because unable to "
                                            "find the bmc error log object "
                                            "path for the given "
                                            "deconfiguration EID [{}] which "
                                            "isolated the hardware [{}]",
                                            eId, hwInventoryPath->str)
                                            .c_str());
                                    error_log::createErrorLog(
                                        error_log::HwIsolationGenericErrMsg,
                                        error_log::Level::Informational,
                                        error_log::CollectTraces);
                                    continue;
                                }
                                eventErrLogPath = logObjPath->str;
                            }
                            else
                            {
                                /**
                                 * Event is required since the hardware is
                                 * temporarily isolated by the respective
                                 * deconfigured reason.
                                 */
                                auto dfgReason = openpower_hw_status::
                                    convertDeconfiguredByReasonFromEnum(
                                        static_cast<openpower_hw_status::
                                                        DeconfiguredByReason>(
                                            hwasState.deconfiguredByEid));
                                eventMsg = std::get<0>(dfgReason);
                                eventSeverity = std::get<1>(dfgReason);
                            }
                        }

                        auto eventObjPath =
                            createEvent(eventSeverity, eventMsg,
                                        hwInventoryPath->str, eventErrLogPath);

                        if (!eventObjPath.has_value())
                        {
                            log<level::ERR>(
                                fmt::format(
                                    "Skipping to create the hardware "
                                    "status event because unable to create "
                                    "the event object for the given hardware "
                                    "[{}]",
                                    hwInventoryPath->str)
                                    .c_str());
                            error_log::createErrorLog(
                                error_log::HwIsolationGenericErrMsg,
                                error_log::Level::Informational,
                                error_log::CollectTraces);
                            continue;
                        }
                    }
                }
                catch (const std::exception& e)
                {
                    log<level::ERR>(
                        fmt::format("Exception [{}], skipping to create "
                                    "the hardware status event for the given "
                                    "hardware [{}]",
                                    e.what(), pdbg_target_path(tgt))
                            .c_str());
                    error_log::createErrorLog(
                        error_log::HwIsolationGenericErrMsg,
                        error_log::Level::Informational,
                        error_log::CollectTraces);
                    continue;
                }
            }
        });
}

void Manager::clearHwStatusEventIfexists(const std::string& hwInventoryPath)
{
    std::erase_if(_hwStatusEvents, [this, hwInventoryPath](const auto& ele) {
        for (const auto& assocEle : ele.second->associations())
        {
            if ((std::get<0>(assocEle) == "event_indicator") &&
                (std::get<2>(assocEle) == hwInventoryPath))
            {
                return true;
            }
        }

        return false;
    });
}

void Manager::handleDeallocatedHw()
{
    auto deallocatedHw = std::move(_deallocatedHwHandler.front());
    _deallocatedHwHandler.pop();
    if (deallocatedHw.second->isEnabled())
    {
        deallocatedHw.second->setEnabled(false);
    }

    auto isolatedhwRecordInfo = _hwIsolationRecordMgr.getIsolatedHwRecordInfo(
        std::string(deallocatedHw.first));

    if (!isolatedhwRecordInfo.has_value())
    {
        // No action, just deconfigured without
        // hardware isolation record
        return;
    }

    log<level::INFO>(fmt::format("{} is deallocated at the host runtime",
                                 deallocatedHw.first)
                         .c_str());

    record::entry::EntryErrLogPath eventErrLogPath =
        std::get<1>(*isolatedhwRecordInfo);

    auto hwStatusInfo =
        getIsolatedHwStatusInfo(std::get<0>(*isolatedhwRecordInfo));

    event::EventMsg eventMsg = std::get<0>(hwStatusInfo);
    event::EventSeverity eventSeverity = std::get<1>(hwStatusInfo);

    clearHwStatusEventIfexists(deallocatedHw.first);

    auto eventObjPath = createEvent(eventSeverity, eventMsg,
                                    deallocatedHw.first, eventErrLogPath);
    if (!eventObjPath.has_value())
    {
        log<level::ERR>(fmt::format("Failed to create the event for {} "
                                    "that was deallocated at the host "
                                    "runtime",
                                    deallocatedHw.first)
                            .c_str());
        error_log::createErrorLog(error_log::HwIsolationGenericErrMsg,
                                  error_log::Level::Informational,
                                  error_log::CollectTraces);
    }
}

void Manager::onOperationalStatusChange(sdbusplus::message::message& message)
{
    try
    {
        dbus_type::Interface interface;
        dbus_type::Properties properties;

        message.read(interface, properties);

        for (const auto& property : properties)
        {
            if (property.first == "Functional")
            {
                if (const auto* propVal = std::get_if<bool>(&property.second))
                {
                    if (!(*propVal))
                    {
                        _deallocatedHwHandler.emplace(std::make_pair(
                            message.get_path(),
                            std::make_unique<sdeventplus::utility::Timer<
                                sdeventplus::ClockId::Monotonic>>(
                                _eventLoop,
                                std::bind(std::mem_fn(
                                              &hw_isolation::event::hw_status::
                                                  Manager::handleDeallocatedHw),
                                          this),
                                std::chrono::seconds(5))));
                    }
                }
                else
                {
                    log<level::ERR>(
                        fmt::format(
                            "D-Bus Message signature [{}] "
                            "Failed to read the Functional property value "
                            "while changed",
                            message.get_signature())
                            .c_str());
                    error_log::createErrorLog(
                        error_log::HwIsolationGenericErrMsg,
                        error_log::Level::Informational,
                        error_log::CollectTraces);
                }
                // No need to look other properties
                break;
            }
        }
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>(
            fmt::format(
                "Exception [{}] and D-Bus Message signature [{}] "
                "so failed to get the OperationalStatus properties value "
                "while changed",
                e.what(), message.get_signature())
                .c_str());
        error_log::createErrorLog(error_log::HwIsolationGenericErrMsg,
                                  error_log::Level::Informational,
                                  error_log::CollectTraces);
    }
}

void Manager::watchOperationalStatusChange()
{
    constexpr auto CpuCoreIface = "xyz.openbmc_project.Inventory.Item.CpuCore";
    auto objsToWatch = utils::getChildsInventoryPath(
        _bus, std::string("/xyz/openbmc_project/inventory"), CpuCoreIface);

    if (!objsToWatch.has_value())
    {
        log<level::ERR>(
            fmt::format("Failed to get the {} objects from the inventory "
                        "to watch Functional property",
                        CpuCoreIface)
                .c_str());
        return;
    }

    // Clear old watcher since inventory item objects might be
    // vary if the respective FRU is replaced.
    _watcherOnOperationalStatus.clear();

    for (const auto& objToWatch : *objsToWatch)
    {
        try
        {
            namespace sdbusplus_match = sdbusplus::bus::match;
            _watcherOnOperationalStatus.insert(std::make_pair(
                objToWatch.str,
                std::make_unique<sdbusplus_match::match>(
                    _bus,
                    sdbusplus_match::rules::propertiesChanged(
                        objToWatch.str, "xyz.openbmc_project.State.Decorator."
                                        "OperationalStatus"),
                    std::bind(std::mem_fn(&Manager::onOperationalStatusChange),
                              this, std::placeholders::_1))));
        }
        catch (const std::exception& e)
        {
            // Just log error and continue with next object
            log<level::ERR>(
                fmt::format("Exception [{}] while adding the D-Bus match "
                            "rules for {} to watch OperationalStatus",
                            e.what(), objToWatch.str)
                    .c_str());
            error_log::createErrorLog(error_log::HwIsolationGenericErrMsg,
                                      error_log::Level::Informational,
                                      error_log::CollectTraces);
        }
    }
}

void Manager::onHostStateChange(sdbusplus::message::message& message)
{
    dbus_type::Interface interface;
    dbus_type::Properties properties;

    try
    {
        message.read(interface, properties);

        for (const auto& property : properties)
        {
            if (property.first == "CurrentHostState")
            {
                if (const auto* propVal =
                        std::get_if<std::string>(&property.second))
                {
                    if (*propVal ==
                        "xyz.openbmc_project.State.Host.HostState.Quiesced")
                    {
                        log<level::INFO>(
                            fmt::format("HostState is {}, pull the deconfig "
                                        "reason from the cec device tree.",
                                        *propVal)
                                .c_str());
                        restoreHardwaresStatusEvent();
                    }
                    if (*propVal ==
                        "xyz.openbmc_project.State.Host.HostState.Off")
                    {
                        if (!_watcherOnOperationalStatus.empty())
                        {
                            log<level::INFO>(
                                fmt::format("HostState is {}, remove runtime "
                                            "deallocation watcher.",
                                            *propVal)
                                    .c_str());
                            _watcherOnOperationalStatus.clear();
                        }
                    }
                }
                else
                {
                    log<level::ERR>(
                        fmt::format("D-Bus Message signature [{}] "
                                    "Failed to read the CurrentHostState "
                                    "property value while changed",
                                    message.get_signature())
                            .c_str());
                    error_log::createErrorLog(
                        error_log::HwIsolationGenericErrMsg,
                        error_log::Level::Informational,
                        error_log::CollectTraces);
                }
                // No need to look other properties
                break;
            }
        }
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>(
            fmt::format("Exception [{}] and D-Bus Message signature [{}] "
                        "so failed to get the CurrentHostState property value "
                        "while changed",
                        e.what(), message.get_signature())
                .c_str());
        error_log::createErrorLog(error_log::HwIsolationGenericErrMsg,
                                  error_log::Level::Informational,
                                  error_log::CollectTraces);
    }
}

void Manager::onBootProgressChange(sdbusplus::message::message& message)
{
    dbus_type::Interface interface;
    dbus_type::Properties properties;

    try
    {
        message.read(interface, properties);

        for (const auto& property : properties)
        {
            if (property.first == "BootProgress")
            {
                if (const auto* propVal =
                        std::get_if<std::string>(&property.second))
                {
                    if (*propVal == "xyz.openbmc_project.State.Boot.Progress."
                                    "ProgressStages.SystemInitComplete")
                    {
                        log<level::INFO>(
                            fmt::format("BootProgress is {}, pull the deconfig "
                                        "reason from the cec device tree.",
                                        *propVal)
                                .c_str());
                        restoreHardwaresStatusEvent();
                    }
                    else if (*propVal ==
                             "xyz.openbmc_project.State.Boot.Progress."
                             "ProgressStages.OSRunning")
                    {
                        log<level::INFO>(
                            fmt::format("BootProgress is {}, watch "
                                        "Functional property for the runtime "
                                        "deallocation",
                                        *propVal)
                                .c_str());
                        watchOperationalStatusChange();
                    }
                }
                else
                {
                    log<level::ERR>(
                        fmt::format("D-Bus Message signature [{}] "
                                    "Failed to read the BootProgress"
                                    "property value while changed",
                                    message.get_signature())
                            .c_str());
                    error_log::createErrorLog(
                        error_log::HwIsolationGenericErrMsg,
                        error_log::Level::Informational,
                        error_log::CollectTraces);
                }
                // No need to look other properties
                break;
            }
        }
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>(
            fmt::format("Exception [{}] and D-Bus Message signature [{}] "
                        "so failed to get the BootProgress property value "
                        "while changed",
                        e.what(), message.get_signature())
                .c_str());
        error_log::createErrorLog(error_log::HwIsolationGenericErrMsg,
                                  error_log::Level::Informational,
                                  error_log::CollectTraces);
    }
}

bool Manager::isOSRunning()
{
    auto bootProgressStage = utils::getDBusPropertyVal<std::string>(
        _bus, HOST_STATE_OBJ_PATH, "xyz.openbmc_project.State.Boot.Progress",
        "BootProgress");

    if (bootProgressStage == "xyz.openbmc_project.State.Boot.Progress."
                             "ProgressStages.OSRunning")
    {
        return true;
    }

    return false;
}

void Manager::restorePersistedHwIsolationStatusEvent()
{
    auto createEventForPersistedEventFile = [this](const auto& file) {
        auto fileEventId = std::stoul(file.path().filename());

        auto eventObjPath =
            fs::path(HW_STATUS_EVENTS_PATH) / file.path().filename();

        // All members will be filled from persisted file.
        this->_hwStatusEvents.insert(std::make_pair(
            fileEventId,
            std::make_unique<hw_isolation::event::Event>(
                this->_bus, eventObjPath, fileEventId, event::EventSeverity(),
                event::EventMsg(), type::AssociationDef(), true)));

        if (this->_lastEventId < fileEventId)
        {
            this->_lastEventId = fileEventId;
        }
    };

    std::ranges::for_each(
        fs::directory_iterator(
            fs::path(HW_ISOLATION_EVENT_PERSIST_PATH).parent_path()),
        createEventForPersistedEventFile);
}

void Manager::restore()
{
    auto osRunning = isOSRunning();

    restorePersistedHwIsolationStatusEvent();

    if (osRunning)
    {
        watchOperationalStatusChange();
    }
}

} // namespace hw_status
} // namespace event
} // namespace hw_isolation
