// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <string>
#include <vector>

namespace hw_isolation
{
namespace type
{

template <typename... T>
using ServerObject = typename sdbusplus::server::object::object<T...>;

using InstanceId = uint32_t;
using LocationCode = std::string;
using AsscDefFwdType = std::string;
using AsscDefRevType = std::string;
using AssociatedObjPat = std::string;
using AssociationDef =
    std::vector<std::tuple<AsscDefFwdType, AsscDefRevType, AssociatedObjPat>>;

namespace CommonError = sdbusplus::xyz::openbmc_project::Common::Error;

constexpr auto ObjectMapperName = "xyz.openbmc_project.ObjectMapper";
constexpr auto ObjectMapperPath = "/xyz/openbmc_project/object_mapper";

constexpr auto LoggingObjectPath = "/xyz/openbmc_project/logging";
constexpr auto LoggingInterface = "org.open_power.Logging.PEL";

} // namespace type
} // namespace hw_isolation
