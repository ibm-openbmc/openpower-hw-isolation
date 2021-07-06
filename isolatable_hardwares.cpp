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

std::optional<devtree::DevTreePhysPath> IsolatableHWs::getPhysicalPath(
    const sdbusplus::message::object_path& isolateHardware)
{
    try
    {
        auto isolateHwInstanceInfo =
            getInstanceInfo(isolateHardware.filename());
        if (!isolateHwInstanceInfo.has_value())
        {
            return std::nullopt;
        }

        auto isolateHwId = IsolatableHWs::HW_Details::HwId{
            IsolatableHWs::HW_Details::HwId::ItemObjectName(
                isolateHwInstanceInfo->first)};
        auto isolateHwDetails = getIsotableHWDetails(isolateHwId);

        if (!isolateHwDetails.has_value())
        {
            log<level::ERR>(
                fmt::format("Given isolate hardware object name [{}] "
                            "is not found in isolatable hardware list",
                            isolateHardware.filename())
                    .c_str());
            return std::nullopt;
        }

        struct pdbg_target* isolateHwTarget;
        devtree::lookup_func::CanGetPhysPath canGetPhysPath{false};

        if (isolateHwDetails->second._isItFRU)
        {
            auto unExpandedLocCode{devtree::getUnexpandedLocCode(
                getLocationCode(isolateHardware))};
            if (!unExpandedLocCode.has_value())
            {
                return std::nullopt;
            }

            pdbg_for_each_class_target(
                isolateHwDetails->first._pdbgClassName._name.c_str(),
                isolateHwTarget)
            {
                canGetPhysPath = isolateHwDetails->second._physPathFuncLookUp(
                    isolateHwTarget, isolateHwInstanceInfo->second,
                    *unExpandedLocCode);

                if (canGetPhysPath)
                {
                    break;
                }
            }
        }
        else
        {
            auto parentFruObjPath = getParentFruObjPath(
                isolateHardware,
                isolateHwDetails->second._parentFruHwId._itemObjectName);
            if (!parentFruObjPath.has_value())
            {
                return std::nullopt;
            }
            auto parentFruInstanceInfo =
                getInstanceInfo(parentFruObjPath->filename());
            if (!parentFruInstanceInfo.has_value())
            {
                return std::nullopt;
            }
            auto parentFruHwDetails =
                getIsotableHWDetails(isolateHwDetails->second._parentFruHwId);
            if (!parentFruHwDetails.has_value())
            {
                log<level::ERR>(
                    fmt::format("Parent fru details for the given isolate "
                                "hardware object name [{}] is not found in "
                                "isolatable hardware list",
                                isolateHardware.filename())
                        .c_str());
                return std::nullopt;
            }

            auto unExpandedLocCode{devtree::getUnexpandedLocCode(
                getLocationCode(*parentFruObjPath))};

            if (!unExpandedLocCode.has_value())
            {
                return std::nullopt;
            }

            struct pdbg_target* parentFruTarget;

            pdbg_for_each_class_target(
                parentFruHwDetails->first._pdbgClassName._name.c_str(),
                parentFruTarget)
            {
                canGetPhysPath = parentFruHwDetails->second._physPathFuncLookUp(
                    parentFruTarget, parentFruInstanceInfo->second,
                    *unExpandedLocCode);

                if (!canGetPhysPath)
                {
                    continue;
                }

                pdbg_for_each_target(
                    isolateHwDetails->first._pdbgClassName._name.c_str(),
                    parentFruTarget, isolateHwTarget)
                {
                    canGetPhysPath =
                        isolateHwDetails->second._physPathFuncLookUp(
                            isolateHwTarget, isolateHwInstanceInfo->second,
                            *unExpandedLocCode);

                    if (canGetPhysPath)
                    {
                        break;
                    }
                }

                // No use to check other parent, since it will enter here
                // if identified the parent or,
                // if found the isolate hardware to get physical path or,
                // if not found the isolate hardware to get physical path
                if (canGetPhysPath)
                {
                    break;
                }
            }
        }

        if (!canGetPhysPath)
        {
            log<level::ERR>(fmt::format("Given hardware [{}] is not found "
                                        " in phal cec device tree",
                                        isolateHardware.str)
                                .c_str());
            return std::nullopt;
        }

        return devtree::getPhysicalPath(isolateHwTarget);
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(fmt::format("Exception [{}]", e.what()).c_str());
        return std::nullopt;
    }
}

std::optional<std::vector<sdbusplus::message::object_path>>
    IsolatableHWs::getInventoryPathsByLocCode(
        const LocationCode& unexpandedLocCode)
{
    constexpr auto vpdMgrObjPath = "/com/ibm/VPD/Manager";
    constexpr auto vpdInterface = "com.ibm.VPD.Manager";

    std::vector<sdbusplus::message::object_path> listOfInventoryObjPaths;

    try
    {
        auto dbusServiceName =
            utils::getDBusServiceName(_bus, vpdMgrObjPath, vpdInterface);

        auto method = _bus.new_method_call(dbusServiceName.c_str(),
                                           vpdMgrObjPath, vpdInterface,
                                           "GetFRUsByUnexpandedLocationCode");

        // passing 0 as node number
        // FIXME if enabled multi node system
        method.append(unexpandedLocCode, static_cast<uint16_t>(0));

        auto resp = _bus.call(method);

        resp.read(listOfInventoryObjPaths);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(fmt::format("Exception [{}] to get inventory path for "
                                    "the given locationc code [{}]",
                                    e.what(), unexpandedLocCode)
                            .c_str());
        return std::nullopt;
    }

    return listOfInventoryObjPaths;
}

} // namespace isolatable_hws
} // namespace hw_isolation
