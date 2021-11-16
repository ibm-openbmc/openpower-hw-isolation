// SPDX-License-Identifier: Apache-2.0

#include "config.h"

#include "hw_isolation_event/hw_status_manager.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>

#include <filesystem>

namespace hw_isolation
{
namespace event
{
namespace hw_status
{

using namespace phosphor::logging;
namespace fs = std::filesystem;

constexpr auto HW_STATUS_EVENTS_PATH =
    HW_ISOLATION_OBJPATH "/events/hw_isolation_status";

Manager::Manager(sdbusplus::bus::bus& bus) : _bus(bus), _lastEventId(0)
{}

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

} // namespace hw_status
} // namespace event
} // namespace hw_isolation
