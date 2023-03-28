#pragma once
#include <nlohmann/json.hpp>
#include <sdbusplus/bus.hpp>

namespace openpower::faultlog
{
/**
 * @class FaultLogPolicy
 *
 * Capture faultlog policy and FCO value
 */
class FaultLogPolicy
{
  private:
    FaultLogPolicy() = delete;
    FaultLogPolicy(const FaultLogPolicy&) = delete;
    FaultLogPolicy& operator=(const FaultLogPolicy&) = delete;
    FaultLogPolicy(FaultLogPolicy&&) = delete;
    FaultLogPolicy& operator=(FaultLogPolicy&&) = delete;
    virtual ~FaultLogPolicy() = default;

  public:
    /** @brief Populate hardware isolation policy and FCO value
     *
     *  @param[in] bus - D-Bus to attach to
     *  @param[in] json - mag JSON file
     *  @return 1 on failure and 0 on success
     */
    static int populate(sdbusplus::bus::bus& bus, nlohmann::json& nagJson);
};
} // namespace openpower::faultlog
