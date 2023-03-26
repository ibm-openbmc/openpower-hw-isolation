#pragma once

#include <nlohmann/json.hpp>
#include <sdbusplus/bus.hpp>

namespace openpower::faultlog
{
/**
 * @class ServicableRecords
 *
 * Capture hardware isolation servicable records in JSON file
 */
class ServicableRecords
{
  private:
    ServicableRecords() = delete;
    ServicableRecords(const ServicableRecords&) = delete;
    ServicableRecords& operator=(const ServicableRecords&) = delete;
    ServicableRecords(ServicableRecords&&) = delete;
    ServicableRecords& operator=(ServicableRecords&&) = delete;
    virtual ~ServicableRecords() = default;

  public:
    /** @brief Parse through the error log object and update the NAG json
     *
     *  @param[in] bus - D-Bus to attach to
     *  @param[in] errorLog - D-Bus error log object to process data
     *  @param[in] jsonServEvent - Update JSON servicable event
     */
    static void populate(sdbusplus::bus::bus& bus,
                         sdbusplus::message::object_path& errorLog,
                         nlohmann::json& jsonServEvent);
};
} // namespace openpower::faultlog
