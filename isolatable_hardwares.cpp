// SPDX-License-Identifier: Apache-2.0

#include "isolatable_hardwares.hpp"

#include "utils.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>

namespace hw_isolation
{
namespace isolatable_hws
{

using namespace phosphor::logging;

IsolatableHWs::IsolatableHWs(sdbusplus::bus::bus& bus) : _bus(bus)
{
    /**
     * @brief HwId consists with below ids.
     *
     * 1 - The inventory item interface name
     * 2 - The inventory item object name
     * 3 - The pdbg class name
     */
    // The below HwIds will be used to many units as parent fru
    // so creating one object which can reuse.
    IsolatableHWs::HW_Details::HwId processorHwId(
        "xyz.openbmc_project.Inventory.Item.Cpu", "cpu", "proc");
    IsolatableHWs::HW_Details::HwId dimmHwId(
        "xyz.openbmc_project.Inventory.Item.Dimm", "dimm", "dimm");
    IsolatableHWs::HW_Details::HwId emptyHwId("", "", "");
    bool ItIsFRU = true;

    _isolatableHWsList = {
        {dimmHwId, IsolatableHWs::HW_Details(
                       ItIsFRU, emptyHwId, devtree::lookup_func::locationCode)},

        {processorHwId, IsolatableHWs::HW_Details(ItIsFRU, emptyHwId,
                                                  devtree::lookup_func::mruId)},

        {IsolatableHWs::HW_Details::HwId(
             "xyz.openbmc_project.Inventory.Item.CpuCore", "core", "core"),
         IsolatableHWs::HW_Details(!ItIsFRU, processorHwId,
                                   devtree::lookup_func::chipUnitPos)},
    };
}

std::optional<
    std::pair<IsolatableHWs::HW_Details::HwId::ItemObjectName, InstanceId>>
    IsolatableHWs::getInstanceInfo(const std::string& dbusObjName) const
{
    try
    {
        std::string hwObjName(dbusObjName);

        std::string::iterator it =
            std::find_if(hwObjName.begin(), hwObjName.end(),
                         [](const char chr) { return std::isdigit(chr); });

        IsolatableHWs::HW_Details::HwId::ItemObjectName hwInstanceName(
            hwObjName.substr(0, std::distance(hwObjName.begin(), it)));
        InstanceId hwInstanceId{0xFFFFFFFF};
        if (it != hwObjName.end())
        {
            hwInstanceId = std::stoi(
                hwObjName.substr(std::distance(hwObjName.begin(), it)));
        }
        return std::make_pair(hwInstanceName, hwInstanceId);
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(
            fmt::format("Exception [{}] to get instance details from "
                        " given the Dbus object name [{}]",
                        e.what(), dbusObjName)
                .c_str());
    }
    return std::nullopt;
}

std::optional<
    std::pair<IsolatableHWs::HW_Details::HwId, IsolatableHWs::HW_Details>>
    IsolatableHWs::getIsotableHWDetails(
        const IsolatableHWs::HW_Details::HwId& id) const
{

    auto it = std::find_if(
        _isolatableHWsList.begin(), _isolatableHWsList.end(),
        [&id](const auto& isolatableHw) { return isolatableHw.first == id; });

    if (it != _isolatableHWsList.end())
    {
        return *it;
    }
    return std::nullopt;
}

LocationCode IsolatableHWs::getLocationCode(
    const sdbusplus::message::object_path& dbusObjPath)
{
    return utils::getDBusPropertyVal<LocationCode>(
        _bus, dbusObjPath, "com.ibm.ipzvpd.Location", "LocationCode");
}

std::optional<sdbusplus::message::object_path>
    IsolatableHWs::getParentFruObjPath(
        const sdbusplus::message::object_path& isolateHardware,
        const IsolatableHWs::HW_Details::HwId::ItemObjectName&
            parentFruObjectName) const
{
    size_t startPosOfFruObj =
        isolateHardware.str.find(parentFruObjectName._name);
    if (startPosOfFruObj == std::string::npos)
    {
        log<level::ERR>(
            fmt::format("Failed to get parent fru object [{}] "
                        "path for isolate hardware object path [{}].",
                        parentFruObjectName._name, isolateHardware.str)
                .c_str());
        return std::nullopt;
    }

    size_t endPosOfFruObj = isolateHardware.str.find("/", startPosOfFruObj);
    if (endPosOfFruObj == std::string::npos)
    {
        log<level::ERR>(
            fmt::format("Failed to get parent fru object [{}] "
                        "path for isolate hardware object path [{}].",
                        parentFruObjectName._name, isolateHardware.str)
                .c_str());
        return std::nullopt;
    }

    return isolateHardware.str.substr(0, endPosOfFruObj);
}

} // namespace isolatable_hws
} // namespace hw_isolation
