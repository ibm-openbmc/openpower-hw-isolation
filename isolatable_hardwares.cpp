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
                       ItIsFRU, emptyHwId, devtree::lookup_func::locationCode,
                       inv_path_lookup_func::itemObjName)},

        {processorHwId, IsolatableHWs::HW_Details(
                            ItIsFRU, emptyHwId, devtree::lookup_func::mruId,
                            inv_path_lookup_func::itemInstance)},

        {IsolatableHWs::HW_Details::HwId(
             "xyz.openbmc_project.Inventory.Item.CpuCore", "core", "core"),
         IsolatableHWs::HW_Details(!ItIsFRU, processorHwId,
                                   devtree::lookup_func::chipUnitPos,
                                   inv_path_lookup_func::itemInstance)},
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

std::optional<struct pdbg_target*>
    IsolatableHWs::getParentFruPhalDevTreeTgt(struct pdbg_target* devTreeTgt)
{
    /**
     * FIXME: Today, All FRU parts (units - both(chiplet and non-chiplet))
     *        are modelled under the respective processor in cec device
     *        tree so, if something changed then, need to revisit the
     *        logic which is used to get the FRU details of FRU unit.
     */
    struct pdbg_target* parentFruTarget =
        pdbg_target_parent("proc", devTreeTgt);
    std::string fruUnitDevTreePath = pdbg_target_path(devTreeTgt);
    if (parentFruTarget == nullptr)
    {
        log<level::ERR>(
            fmt::format("Failed to get the processor target from phal cec "
                        "device tree for the given target [{}]",
                        fruUnitDevTreePath)
                .c_str());
        return std::nullopt;
    }
    return parentFruTarget;
}

std::optional<std::vector<sdbusplus::message::object_path>>
    IsolatableHWs::getChildsInventoryPath(
        const sdbusplus::message::object_path& parentObjPath,
        const std::string& interfaceName)
{
    std::vector<sdbusplus::message::object_path> listOfChildsInventoryPath;

    try
    {
        auto dbusServiceName = utils::getDBusServiceName(
            _bus, type::objectMapperPath, type::objectMapperName);

        auto method = _bus.new_method_call(
            dbusServiceName.c_str(), type::objectMapperPath,
            type::objectMapperName, "GetSubTreePaths");

        method.append(parentObjPath, 0, interfaceName);

        auto resp = _bus.call(method);

        resp.read(listOfChildsInventoryPath);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(
            fmt::format("Exception [{}] to get childs inventory path "
                        "for given objPath[{}] interface[{}]",
                        e.what(), parentObjPath.str, interfaceName)
                .c_str());
        return std::nullopt;
    }
    return listOfChildsInventoryPath;
}

std::optional<sdbusplus::message::object_path> IsolatableHWs::getInventoryPath(
    const devtree::DevTreePhysPath& physicalPath)
{
    try
    {
        auto isolatedHwTgt = devtree::getPhalDevTreeTgt(physicalPath);
        if (!isolatedHwTgt.has_value())
        {
            return std::nullopt;
        }
        auto isolatedHwTgtDevTreePath = pdbg_target_path(*isolatedHwTgt);

        std::string isolatedHwPdbgClass =
            pdbg_target_class_name(*isolatedHwTgt);

        auto isolatedHwId = IsolatableHWs::HW_Details::HwId{
            IsolatableHWs::HW_Details::HwId::PhalPdbgClassName(
                isolatedHwPdbgClass)};
        auto isolatedHwDetails = getIsotableHWDetails(isolatedHwId);

        if (!isolatedHwDetails.has_value())
        {
            log<level::ERR>(
                fmt::format("Isolated hardware [{}] pdbg class [{}] is "
                            "not found in isolatable hardware list",
                            isolatedHwTgtDevTreePath, isolatedHwPdbgClass)
                    .c_str());
            return std::nullopt;
        }

        sdbusplus::message::object_path isolatedHwInventoryPath;
        if (isolatedHwDetails->second._isItFRU)
        {
            auto isolatedHwInfo = devtree::getFRUDetails(*isolatedHwTgt);
            auto isolateHw = isolatedHwDetails->first._itemObjectName._name +
                             (isolatedHwInfo.second == 0xFFFFFFFF
                                  ? ""
                                  : std::to_string(isolatedHwInfo.second));

            auto inventoryPathList =
                getInventoryPathsByLocCode(isolatedHwInfo.first);

            if (!inventoryPathList.has_value())
            {
                return std::nullopt;
            }

            auto isolateHwPath = std::find_if(
                inventoryPathList->begin(), inventoryPathList->end(),
                [&isolateHw, &isolatedHwDetails, this](const auto& path) {
                    return isolatedHwDetails->second._invPathFuncLookUp(
                        path, isolateHw, this->_bus);
                });

            if (isolateHwPath == inventoryPathList->end())
            {
                log<level::ERR>(fmt::format("Failed to get inventory path for "
                                            "given device path [{}]",
                                            isolatedHwTgtDevTreePath)
                                    .c_str());
                return std::nullopt;
            }

            isolatedHwInventoryPath = *isolateHwPath;
        }
        else
        {
            auto parentFruTgt = getParentFruPhalDevTreeTgt(*isolatedHwTgt);
            if (!parentFruTgt.has_value())
            {
                return std::nullopt;
            }

            std::string parentFruTgtPdbgClass =
                pdbg_target_class_name(*parentFruTgt);
            auto parentFruHwId = IsolatableHWs::HW_Details::HwId{
                IsolatableHWs::HW_Details::HwId::PhalPdbgClassName(
                    parentFruTgtPdbgClass)};

            auto parentFruHwDetails = getIsotableHWDetails(parentFruHwId);
            if (!parentFruHwDetails.has_value())
            {
                log<level::ERR>(
                    fmt::format(
                        "Isolated hardware [{}] parent fru pdbg "
                        "class [{}] is not found in isolatable hardware list",
                        isolatedHwTgtDevTreePath, parentFruTgtPdbgClass)
                        .c_str());
                return std::nullopt;
            }

            auto parentFruHwInfo = devtree::getFRUDetails(*parentFruTgt);
            auto parentFruHw = parentFruHwDetails->first._itemObjectName._name +
                               (parentFruHwInfo.second == 0xFFFFFFFF
                                    ? ""
                                    : std::to_string(parentFruHwInfo.second));

            auto parentFruInventoryPathList =
                getInventoryPathsByLocCode(parentFruHwInfo.first);
            if (!parentFruInventoryPathList.has_value())
            {
                return std::nullopt;
            }

            auto parentFruPath = std::find_if(
                parentFruInventoryPathList->begin(),
                parentFruInventoryPathList->end(),
                [&parentFruHw, &parentFruHwDetails, this](const auto& path) {
                    return parentFruHwDetails->second._invPathFuncLookUp(
                        path, parentFruHw, this->_bus);
                });
            if (parentFruPath == parentFruInventoryPathList->end())
            {
                log<level::ERR>(
                    fmt::format("Failed to get get parent fru inventory path "
                                "for given device path [{}]",
                                isolatedHwTgtDevTreePath)
                        .c_str());
                return std::nullopt;
            }

            auto childsInventoryPath = getChildsInventoryPath(
                *parentFruPath, isolatedHwDetails->first._interfaceName._name);

            if (!childsInventoryPath.has_value())
            {
                return std::nullopt;
            }

            auto isolateHwInstId =
                devtree::getHwInstIdFromDevTree(*isolatedHwTgt);

            auto isolateHw = isolatedHwDetails->first._itemObjectName._name +
                             (isolateHwInstId == 0xFFFFFFFF
                                  ? ""
                                  : std::to_string(isolateHwInstId));

            auto isolateHwPath = std::find_if(
                childsInventoryPath->begin(), childsInventoryPath->end(),
                [&isolateHw, &isolatedHwDetails, this](const auto& path) {
                    return isolatedHwDetails->second._invPathFuncLookUp(
                        path, isolateHw, this->_bus);
                });

            if (isolateHwPath == childsInventoryPath->end())
            {
                log<level::ERR>(fmt::format("Failed to get inventory path for "
                                            "given device path [{}]",
                                            isolatedHwTgtDevTreePath)
                                    .c_str());
                return std::nullopt;
            }

            isolatedHwInventoryPath = *isolateHwPath;
        }
        return isolatedHwInventoryPath;
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(fmt::format("Exception [{}]", e.what()).c_str());
        return std::nullopt;
    }
}

} // namespace isolatable_hws

namespace inv_path_lookup_func
{

IsItIsoHwInvPath itemObjName(const sdbusplus::message::object_path& objPath,
                             const std::string& instance,
                             sdbusplus::bus::bus& bus)
{
    return objPath.filename().find(instance) != std::string::npos;
}

IsItIsoHwInvPath itemInstance(const sdbusplus::message::object_path& objPath,
                              const std::string& instance,
                              sdbusplus::bus::bus& bus)
{
    return objPath.filename() == instance;
}

} // namespace inv_path_lookup_func

} // namespace hw_isolation
