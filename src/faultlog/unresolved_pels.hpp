#pragma once

#include <libguard/include/guard_record.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/bus.hpp>

namespace openpower::faultlog
{
using ::openpower::guard::GuardRecords;

/**
 * @class UnresolvedPELs
 *
 *  Captures all unresolved PELS with deconfig/guard bit set
 */
class UnresolvedPELs
{
  private:
    UnresolvedPELs() = delete;
    UnresolvedPELs(const UnresolvedPELs&) = delete;
    UnresolvedPELs& operator=(const UnresolvedPELs&) = delete;
    UnresolvedPELs(UnresolvedPELs&&) = delete;
    UnresolvedPELs& operator=(UnresolvedPELs&&) = delete;
    ~UnresolvedPELs() = delete;

  public:
    /** @brief Get count of unresolved pels with deconfig bit set
     *  @param[in] bus - D-Bus to attach to
     *  @param[in] ignorePwrFanPel - flag to add power/fan pel
     *
     *  @return 0 if no records are found else count of records
     */
    static int getCount(sdbusplus::bus::bus& bus, bool ignorePwrFanPel);

    /** @brief Populate unresolved PEL's serviceable events to NAG json
     *
     *  @param[in] bus - D-Bus to attach to
     *  @param[in] guardRecords - hardware isolated records to parse
     *  @param[in/out] jsonNag - Json file capturing serviceable events
     *
     *  @return void
     */
    static void populate(sdbusplus::bus::bus& bus,
                         const GuardRecords& guardRecords,
                         nlohmann::json& jsonNag);
};
} // namespace openpower::faultlog
