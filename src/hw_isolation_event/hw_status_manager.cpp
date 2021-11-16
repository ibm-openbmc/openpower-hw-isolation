// SPDX-License-Identifier: Apache-2.0

extern "C"
{
#include <libpdbg.h>
}

#include "config.h"

#include "attributes_info.H"

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
using PropertyValue = std::variant<std::string>;
using Properties = std::map<Property, PropertyValue>;
using Interfaces = std::map<Interface, Properties>;
} // namespace dbus_type

using namespace phosphor::logging;
namespace fs = std::filesystem;

constexpr auto HW_STATUS_EVENTS_PATH =
    HW_ISOLATION_OBJPATH "/events/hw_isolation_status";

Manager::Manager(sdbusplus::bus::bus& bus,
                 record::Manager& hwIsolationRecordMgr) :
    _bus(bus),
    _lastEventId(0), _isolatableHWs(bus),
    _hwIsolationRecordMgr(hwIsolationRecordMgr),
    _requiredHwsPdbgClass({"dimm", "fc"})
{
    // Adding the required D-Bus match rules to create hardware status event
    // if interested signal is occurred.
    try
    {
        constexpr auto HOST_STATE_OBJ_PATH = "/xyz/openbmc_project/state/host0";
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
            commit<type::CommonError::InternalFailure>(
                type::ErrorLogLevel::Informational);
            return std::make_pair("Unknown", event::EventSeverity::Warning);
    }
}

void Manager::restoreHardwaresStatusEvent()
{
    clearHardwaresStatusEvent();

    std::for_each(
        _requiredHwsPdbgClass.begin(), _requiredHwsPdbgClass.end(),
        [this](const auto& ele) {
            struct pdbg_target* tgt;
            pdbg_for_each_class_target(ele.c_str(), tgt)
            {
                try
                {
                    ATTR_HWAS_STATE_Type hwasState;
                    if (DT_GET_PROP(ATTR_HWAS_STATE, tgt, hwasState))
                    {
                        log<level::ERR>(
                            fmt::format("Skipping to create the hardware "
                                        "status event because failed to get "
                                        "ATTR_HWAS_STATE from [{}]",
                                        pdbg_target_path(tgt))
                                .c_str());
                        commit<type::CommonError::InternalFailure>(
                            type::ErrorLogLevel::Informational);
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
                            commit<type::CommonError::InternalFailure>(
                                type::ErrorLogLevel::Informational);
                            continue;
                        }

                        devtree::DevTreePhysPath devTreePhysPath;
                        std::copy(std::begin(physBinPath),
                                  std::end(physBinPath),
                                  std::back_inserter(devTreePhysPath));

                        auto hwInventoryPath =
                            _isolatableHWs.getInventoryPath(devTreePhysPath);

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
                            commit<type::CommonError::InternalFailure>(
                                type::ErrorLogLevel::Informational);
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
                                /**
                                 * Event is not required since the hardware
                                 * isolation record is exist and not applied
                                 * so far.
                                 */
                                continue;
                            }

                            // Error log might be present or not in the record.
                            eventErrLogPath =
                                std::get<1>(*isolatedhwRecordInfo);

                            auto hwStatusInfo = getIsolatedHwStatusInfo(
                                std::get<0>(*isolatedhwRecordInfo));

                            eventMsg = std::get<0>(hwStatusInfo);
                            eventSeverity = std::get<1>(hwStatusInfo);
                        }
                        else if (!isolatedhwRecordInfo.has_value() &&
                                 hwasState.functional)
                        {
                            /**
                             * Event is not required since the hardware
                             * isolation record is not exist and functional.
                             */
                            continue;
                        }
                        else if ((hwasState.deconfiguredByEid &
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

                            auto logObjPath = utils::getBMCLogPath(_bus, eId);
                            if (!logObjPath.has_value())
                            {
                                log<level::ERR>(
                                    fmt::format(
                                        "Skipping to create the hardware "
                                        "status event because unable to "
                                        "find the bmc error log object path "
                                        "for the given deconfiguration EID "
                                        "[{}] which isolated the hardware ",
                                        "[{}]", eId, hwInventoryPath->str)
                                        .c_str());
                                commit<type::CommonError::InternalFailure>(
                                    type::ErrorLogLevel::Informational);
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
                            commit<type::CommonError::InternalFailure>(
                                type::ErrorLogLevel::Informational);
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
                    commit<type::CommonError::InternalFailure>(
                        type::ErrorLogLevel::Informational);
                    continue;
                }
            }
        });
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
                        restoreHardwaresStatusEvent();
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
                        restoreHardwaresStatusEvent();
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
    }
}

} // namespace hw_status
} // namespace event
} // namespace hw_isolation
