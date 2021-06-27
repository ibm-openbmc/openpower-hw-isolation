// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <sdbusplus/server/object.hpp>

namespace hw_isolation
{
namespace type
{

template <typename... T>
using ServerObject = typename sdbusplus::server::object::object<T...>;

} // namespace type
} // namespace hw_isolation
