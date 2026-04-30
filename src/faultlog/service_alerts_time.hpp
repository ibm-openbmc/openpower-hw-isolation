#pragma once

#include <libguard/include/guard_record.hpp>
#include <sdbusplus/bus.hpp>

#include <optional>

namespace openpower::faultlog
{
using ::openpower::guard::GuardRecords;

/**
 * @brief Check if service alerts are enabled
 *
 * @param[in] bus - D-Bus to attach to
 * @return true if enabled, false if disabled (defaults to true on error)
 */
bool isSendServiceAlertsEnabled(sdbusplus::bus::bus& bus);

/**
 * @brief Write current timestamp to file when alerts are disabled
 *
 * @param[in] filePath - Path to timestamp file (optional, uses default if not
 * provided)
 */
void writeDisabledTimestamp(const std::string& filePath = "");

/**
 * @brief Read the timestamp when alerts were disabled
 *
 * @param[in] filePath - Path to timestamp file (optional, uses default if not
 * provided)
 * @return std::optional<time_t> timestamp or nullopt if file doesn't exist
 */
std::optional<time_t> readDisabledTimestamp(const std::string& filePath = "");

/**
 * @brief Check for new unresolved PELs created after service alerts were
 * disabled
 *
 * This function queries the logging service for unresolved Platform Event Logs
 * (PELs) that have the deconfig bit set and were created after the specified
 * disabled time. It filters out guarded and hidden PELs as they are handled
 * separately.
 *
 * @param[in] bus - D-Bus connection to use for querying logging service
 * @param[in] disabledTime - Timestamp (in seconds since epoch) when service
 *                           alerts were disabled. PELs created after this time
 *                           are considered "new"
 *
 * @return true if at least one new unresolved PEL with deconfig bit set is
 *         found after the disabled time, false otherwise
 */
bool checkUnresolvedPELs(sdbusplus::bus::bus& bus, uint64_t disabledTime);

/**
 * @brief Re-enable service alerts when new errors are detected
 *
 * @param[in] bus - D-Bus to attach to
 */
void enableServiceAlerts(sdbusplus::bus::bus& bus);

} // namespace openpower::faultlog
// Made with Bob
