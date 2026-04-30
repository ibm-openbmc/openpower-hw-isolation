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

} // namespace openpower::faultlog

// Made with Bob
