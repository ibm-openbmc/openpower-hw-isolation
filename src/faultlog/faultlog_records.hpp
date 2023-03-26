#pragma once

#include <nlohmann/json.hpp>
#include <sdbusplus/bus.hpp>

namespace openpower::faultlog
{
/**
 * @class FaultLogRecords
 *
 * Capture all hardware isolation records to the NAG json file
 */
class FaultLogRecords
{
  private:
    FaultLogRecords() = delete;
    FaultLogRecords(const FaultLogRecords&) = delete;
    FaultLogRecords& operator=(const FaultLogRecords&) = delete;
    FaultLogRecords(FaultLogRecords&&) = delete;
    FaultLogRecords& operator=(FaultLogRecords&&) = delete;
    virtual ~FaultLogRecords() = default;

  public:
    /** @brief Parse through hardware isolation records and fill NAG json
     *
     *  @param[in] bus - D-Bus to attach to
     *  @param[in] json - mag JSON file
     *  @param[in] processedEid - Update with processed errorlog objects
     *  @return 1 on failure and 0 on success
     */
    static int populate(sdbusplus::bus::bus& bus, nlohmann::json& nagJson,
                        std::vector<int>& processedEid);
};
} // namespace openpower::faultlog
