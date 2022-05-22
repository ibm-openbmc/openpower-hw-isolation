// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common/common_types.hpp"

#include <cereal/access.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/vector.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Logging/Event/server.hpp>

namespace hw_isolation
{
namespace event
{

using EventInterface = sdbusplus::xyz::openbmc_project::Logging::server::Event;
using EventId = uint32_t;
using EventSeverity =
    sdbusplus::xyz::openbmc_project::Logging::server::Event::SeverityLevel;
using EventMsg = std::string;

using AssociationDefInterface =
    sdbusplus::xyz::openbmc_project::Association::server::Definitions;

constexpr auto HW_ISOLATION_EVENT_PERSIST_PATH =
    "/var/lib/op-hw-isolation/persistdata/event/hw_status/{}";

/**
 * @class Event
 *
 * @brief Hardware isolation event implementation
 *
 * @details Implemented the below interfaces
 *          xyz.openbmc_project.Logging.Event
 *          xyz.openbmc_project.Association.Definitions
 *
 */
class Event : public type::ServerObject<EventInterface, AssociationDefInterface>
{
  public:
    Event() = delete;
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    Event(Event&&) = delete;
    Event& operator=(Event&&) = delete;
    virtual ~Event();

    /** @brief Constructor to put object onto bus at a dbus path.
     *
     *  @param[in] bus - bus to attach with dbus event object path.
     *  @param[in] objPath - event dbus object path to attach.
     *  @param[in] eventId - the dbus event id.
     *  @param[in] eventSeverity - the severity of the event.
     *  @param[in] eventMsg - the message of the event
     *  @param[in] associationDef - the association to hold other dbus
     *                              object path along with event object.
     *  @param[in] reqDeserialize - Indicates whether need to serialize or
     *                              deserialize. By default it will serialize.
     */
    Event(sdbusplus::bus::bus& bus, const std::string& objPath,
          const EventId eventId, const EventSeverity eventSeverity,
          const EventMsg& eventMsg, const type::AssociationDef& associationDef,
          const bool reqDeserialize = false);

    /**
     * @brief Used to serialize Event members.
     *
     * @return NULL
     */
    void serialize();

    /**
     * @brief Used to Deserialize Event members.
     *
     * @return NULL
     */
    void deserialize();

  private:
    /** @brief Attached bus connection */
    sdbusplus::bus::bus& _bus;

    /** @brief The id of isolated hardware dbus event */
    EventId _eventId;

    /**
     * @brief Allow cereal class access to allow save and load functions
     *        to be private
     */
    friend class cereal::access;

    /**
     * @brief Helper template that is required by Cereal to perform
     *        serialization.
     *
     * @details * TODO: It is a workaround until fix the following issue
     *            ibm-openbmc/dev/issues/3573.
     *
     * @tparam Archive    - Cereal archive type.
     * @param[in] archive - Reference to Cereal archive.
     * @param[in] version - Class version that enables handling
     *                      a serialized data.
     *
     * @return NULL
     */
    template <class Archive>
    void save(Archive& archive, const uint32_t /*version*/) const
    {
        archive(message(), severity(), timestamp(), associations());
    }

    /**
     * @brief Helper template that is required by Cereal to perform
     *        deserialization.
     *
     * @details * TODO: It is a workaround until fix the following issue
     *            ibm-openbmc/dev/issues/3573.
     *
     * @tparam Archive    - Cereal archive type (BinaryInputArchive).
     * @param[in] archive - Reference to Cereal archive.
     * @param[in] version - Class version that enables handling
     *                      a deserialized data.
     *
     * @return NULL
     */
    template <class Archive>
    void load(Archive& archive, const uint32_t /*version*/)
    {
        EventMsg eventMsg;
        EventSeverity eventSeverity;
        uint64_t eventTimeStamp;
        type::AssociationDef eventAssociations;

        // Must be ordered based on the serialization to deserialize.
        archive(eventMsg, eventSeverity, eventTimeStamp, eventAssociations);

        // Skip to send property change signal in the restore path.
        message(eventMsg, true);
        severity(eventSeverity, true);
        timestamp(eventTimeStamp, true);
        associations(eventAssociations, true);
    }

}; // end of Event class

} // namespace event
} // namespace hw_isolation
