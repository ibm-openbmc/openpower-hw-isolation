// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common_types.hpp"

namespace hw_isolation
{
namespace utils
{

/**
 * @brief Get the Dbus service name
 *
 * @param[in] bus - Bus to attach to.
 * @param[in] objPath - Dbus object path.
 * @param[in] interface - Dbus interface name.
 *
 * @return the service name as string on success
 *         throw exception on failure.
 */
std::string getDBusServiceName(sdbusplus::bus::bus& bus,
                               const std::string& objPath,
                               const std::string& interface);

} // namespace utils
} // namespace hw_isolation
