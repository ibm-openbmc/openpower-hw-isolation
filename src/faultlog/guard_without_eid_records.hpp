#pragma once

#include <libguard/include/guard_record.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/bus.hpp>
using ::openpower::guard::GuardRecords;

namespace openpower::faultlog
{
/**
 * @class GuardWithoutEidRecords
 *
 * Captures hardware isolation/guard records that does not have
 * a corresponding error object created. Example: Manual guard
 */
class GuardWithoutEidRecords
{
  private:
    GuardWithoutEidRecords() = delete;
    GuardWithoutEidRecords(const GuardWithoutEidRecords&) = delete;
    GuardWithoutEidRecords& operator=(const GuardWithoutEidRecords&) = delete;
    GuardWithoutEidRecords(GuardWithoutEidRecords&&) = delete;
    GuardWithoutEidRecords& operator=(GuardWithoutEidRecords&&) = delete;
    ~GuardWithoutEidRecords() = delete;

  public:
    /** @brief Get count of guard records without associated error log objects
     *
     *  @param[in] guardRecords - Guard records
     */
    static int getCount(const GuardRecords& guardRecords);

    /** @brief Captured deconfig data in NAG JSON file
     *
     *  @param[in] guardRecords - Guard records
     *  @param[in] jsonNag - Update JSON servicable event
     */
    static void populate(const GuardRecords& guardRecords,
                         nlohmann::json& jsonNag);
};
} // namespace openpower::faultlog
