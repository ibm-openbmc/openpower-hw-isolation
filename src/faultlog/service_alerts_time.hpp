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
 * @brief Check if new hardware errors occurred after alerts were disabled
 *
 * Checks both guard records and unresolved PELs with deconfig bit set
 *
 * @param[in] bus - D-Bus to attach to
 * @param[in] unresolvedRecords - Guard records to check
 * @param[in] disabledTime - Time when alerts were disabled
 * @return true if new errors exist
 */
bool hasNewErrorsSinceDisabled(sdbusplus::bus::bus& bus,
                               const GuardRecords& unresolvedRecords,
                               time_t disabledTime);

/**
 * @brief Re-enable service alerts when new errors are detected
 *
 * @param[in] bus - D-Bus to attach to
 */
void enableServiceAlerts(sdbusplus::bus::bus& bus);

/**
 * @brief Watch for service alerts property changes and update timestamp
 *
 * @param[in] bus - D-Bus to attach to
 */
void watchServiceAlertsProperty(sdbusplus::bus::bus& bus);

/**
 * @brief Check for new unresolved PELs since disabled time (internal helper)
 *
 * @param[in] bus - D-Bus to attach to
 * @param[in] disabledTime - Time when alerts were disabled (in seconds)
 * @return true if new PELs found
 */
bool checkUnresolvedPELs(sdbusplus::bus::bus& bus, uint64_t disabledTime);

} // namespace openpower::faultlog

// Made with Bob
