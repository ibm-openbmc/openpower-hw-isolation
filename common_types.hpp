// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <string>

namespace hw_isolation
{
namespace type
{

template <typename... T>
using ServerObject = typename sdbusplus::server::object::object<T...>;

using InstanceId = uint32_t;
using LocationCode = std::string;

namespace CommonError = sdbusplus::xyz::openbmc_project::Common::Error;

constexpr auto objectMapperName = "xyz.openbmc_project.ObjectMapper";
constexpr auto objectMapperPath = "/xyz/openbmc_project/object_mapper";

} // namespace type
} // namespace hw_isolation
