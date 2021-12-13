// SPDX-License-Identifier: Apache-2.0

#include "common/isolatable_hardwares.hpp"

#include "common/utils.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>

namespace hw_isolation
{
using namespace phosphor::logging;

namespace isolatable_hws
{

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
        // FRU (Field Replaceable Unit) which are present in
        // OpenPOWER based system

        {processorHwId, IsolatableHWs::HW_Details(
                            ItIsFRU, emptyHwId, devtree::lookup_func::mruId,
                            inv_path_lookup_func::itemInstance, "")},

        {dimmHwId, IsolatableHWs::HW_Details(
                       ItIsFRU, emptyHwId, devtree::lookup_func::locationCode,
                       inv_path_lookup_func::itemLocationCode, "")},

        {IsolatableHWs::HW_Details::HwId(
             "xyz.openbmc_project.Inventory.Item.Tpm", "tpm", "tpm"),
         IsolatableHWs::HW_Details(ItIsFRU, emptyHwId,
                                   devtree::lookup_func::locationCode,
                                   inv_path_lookup_func::itemLocationCode, "")},

        // Processor Subunits

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "eq"),
         IsolatableHWs::HW_Details(
             !ItIsFRU, processorHwId, devtree::lookup_func::chipUnitPos,
             inv_path_lookup_func::itemPrettyName, "Quad")},

        // In BMC inventory, Core and FC representing as
        // "Inventory.Item.CpuCore" since both are core and it will model based
        // on the system core mode.
        {IsolatableHWs::HW_Details::HwId(
             "xyz.openbmc_project.Inventory.Item.CpuCore", "core", "fc"),
         IsolatableHWs::HW_Details(!ItIsFRU, processorHwId,
                                   devtree::lookup_func::pdbgIndex,
                                   inv_path_lookup_func::itemInstance, "")},

        {IsolatableHWs::HW_Details::HwId(
             "xyz.openbmc_project.Inventory.Item.CpuCore", "core", "core"),
         IsolatableHWs::HW_Details(!ItIsFRU, processorHwId,
                                   devtree::lookup_func::chipUnitPos,
                                   inv_path_lookup_func::itemInstance, "")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "mc"),
         IsolatableHWs::HW_Details(
             !ItIsFRU, processorHwId, devtree::lookup_func::chipUnitPos,
             inv_path_lookup_func::itemPrettyName, "Memory Controller")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "mi"),
         IsolatableHWs::HW_Details(!ItIsFRU, processorHwId,
                                   devtree::lookup_func::chipUnitPos,
                                   inv_path_lookup_func::itemPrettyName,
                                   "Processor To Memory Buffer Interface")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "mcc"),
         IsolatableHWs::HW_Details(!ItIsFRU, processorHwId,
                                   devtree::lookup_func::chipUnitPos,
                                   inv_path_lookup_func::itemPrettyName,
                                   "Memory Controller Channel")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "omi"),
         IsolatableHWs::HW_Details(!ItIsFRU, processorHwId,
                                   devtree::lookup_func::chipUnitPos,
                                   inv_path_lookup_func::itemPrettyName,
                                   "OpenCAPI Memory Interface")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "pauc"),
         IsolatableHWs::HW_Details(!ItIsFRU, processorHwId,
                                   devtree::lookup_func::chipUnitPos,
                                   inv_path_lookup_func::itemPrettyName,
                                   "POWER Accelerator Unit Controller")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "pau"),
         IsolatableHWs::HW_Details(
             !ItIsFRU, processorHwId, devtree::lookup_func::chipUnitPos,
             inv_path_lookup_func::itemPrettyName, "POWER Accelerator Unit")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "omic"),
         IsolatableHWs::HW_Details(!ItIsFRU, processorHwId,
                                   devtree::lookup_func::chipUnitPos,
                                   inv_path_lookup_func::itemPrettyName,
                                   "OpenCAPI Memory Interface Controller")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "iohs"),
         IsolatableHWs::HW_Details(!ItIsFRU, processorHwId,
                                   devtree::lookup_func::chipUnitPos,
                                   inv_path_lookup_func::itemPrettyName,
                                   "High speed SMP/OpenCAPI Link")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "smpgroup"),
         IsolatableHWs::HW_Details(
             !ItIsFRU, processorHwId, devtree::lookup_func::chipUnitPos,
             inv_path_lookup_func::itemPrettyName, "OBUS End Point")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "pec"),
         IsolatableHWs::HW_Details(
             !ItIsFRU, processorHwId, devtree::lookup_func::chipUnitPos,
             inv_path_lookup_func::itemPrettyName, "PCI Express controllers")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "phb"),
         IsolatableHWs::HW_Details(
             !ItIsFRU, processorHwId, devtree::lookup_func::chipUnitPos,
             inv_path_lookup_func::itemPrettyName, "PCIe host bridge (PHB)")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "nmmu"),
         IsolatableHWs::HW_Details(!ItIsFRU, processorHwId,
                                   devtree::lookup_func::chipUnitPos,
                                   inv_path_lookup_func::itemPrettyName,
                                   "Nest Memory Management Unit")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "nx"),
         IsolatableHWs::HW_Details(
             !ItIsFRU, processorHwId, devtree::lookup_func::mruId,
             inv_path_lookup_func::itemPrettyName, "Accelerator")},

        // Memory (aka DIMM) subunits

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "ocmb"),
         IsolatableHWs::HW_Details(
             !ItIsFRU, dimmHwId, devtree::lookup_func::pdbgIndex,
             inv_path_lookup_func::itemPrettyName, "OpenCAPI Memory Buffer")},

        {IsolatableHWs::HW_Details::HwId("xyz.openbmc_project.Inventory.Item",
                                         "unit", "mem_port"),
         IsolatableHWs::HW_Details(
             !ItIsFRU, dimmHwId, devtree::lookup_func::pdbgIndex,
             inv_path_lookup_func::itemPrettyName, "DDR Memory Port")},
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

std::optional<
    std::pair<IsolatableHWs::HW_Details::HwId, IsolatableHWs::HW_Details>>
    IsolatableHWs::getIsotableHWDetailsByObjPath(
        const sdbusplus::message::object_path& dbusObjPath) const
{
    std::map<std::string, std::vector<std::string>> objServs;

    try
    {
        auto method =
            _bus.new_method_call(type::ObjectMapperName, type::ObjectMapperPath,
                                 type::ObjectMapperName, "GetObject");

        method.append(dbusObjPath.str);
        method.append(std::vector<std::string>({}));

        auto reply = _bus.call(method);
        reply.read(objServs);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>(fmt::format("Exception [{}] to get the given object "
                                    "[{}] interfaces",
                                    e.what(), dbusObjPath.str)
                            .c_str());
        return std::nullopt;
    }

    /**
     * Get only inventory item interface alone so we can minimize
     * iteration to get isolatable hardware details.
     *
     * Note, "xyz.openbmc_project.Inventory.Item" is generic interface
     * so no need to look for that.
     */
    std::vector<std::pair<std::string, std::string>> inventoryItemIfaces;
    std::for_each(objServs.begin(), objServs.end(),
                  [&inventoryItemIfaces](const auto& service) {
                      auto inventoryItemIfaceIt = std::find_if(
                          service.second.begin(), service.second.end(),
                          [](const auto& interface) {
                              return ((interface.find("Inventory.Item") !=
                                       std::string::npos) &&
                                      (!interface.ends_with("Item")));
                          });

                      if (inventoryItemIfaceIt != service.second.end())
                      {
                          inventoryItemIfaces.emplace_back(std::make_pair(
                              service.first, *inventoryItemIfaceIt));
                      }
                  });

    if (inventoryItemIfaces.empty())
    {
        log<level::ERR>(fmt::format("The given object [{}] does not contains "
                                    "any inventory item interface",
                                    dbusObjPath.str)
                            .c_str());
        return std::nullopt;
    }
    else if (inventoryItemIfaces.size() > 1)
    {
        /**
         * FIXME: Assumption is, the OpenBMC project does not allow to host
         *        the same interface by the different services, and more than
         *        one different inventory item interface (since those will be
         *        achieved by the Association) in the same object.
         */
        std::stringstream objData;
        std::for_each(inventoryItemIfaces.begin(), inventoryItemIfaces.end(),
                      [&objData](const auto& ele) {
                          objData << "Service: " << ele.first
                                  << " Iface: " << ele.second << " | ";
                      });

        log<level::ERR>(
            fmt::format("Either the same interface is hosted "
                        "by different services or different inventory item "
                        "interfaces are hosted in the same object [{}]. "
                        "ObjectData [{}]",
                        dbusObjPath.str, objData.str())
                .c_str());
        return std::nullopt;
    }

    auto objHwId{IsolatableHWs::HW_Details::HwId{
        IsolatableHWs::HW_Details::HwId::ItemInterfaceName(
            inventoryItemIfaces[0].second)}};

    // TODO Below decision need to be based on system core mode
    //     i.e whether need to use "fc" (in big core system) or
    //     "core" (in small core system) pdbg target class to get
    //     the appropriate isotable hardware details.
    if (objHwId._interfaceName._name.ends_with("CpuCore"))
    {
        objHwId = IsolatableHWs::HW_Details::HwId{
            IsolatableHWs::HW_Details::HwId::PhalPdbgClassName("fc")};
    }

    return getIsotableHWDetails(objHwId);
}

LocationCode IsolatableHWs::getLocationCode(
    const sdbusplus::message::object_path& dbusObjPath)
{
    return utils::getDBusPropertyVal<LocationCode>(
        _bus, dbusObjPath,
        "xyz.openbmc_project.Inventory.Decorator.LocationCode", "LocationCode");
}

std::optional<sdbusplus::message::object_path>
    IsolatableHWs::getParentFruObjPath(
        const sdbusplus::message::object_path& isolateHardware,
        const IsolatableHWs::HW_Details::HwId::ItemInterfaceName&
            parentFruIfaceName) const
{
    std::map<std::string, std::map<std::string, std::vector<std::string>>>
        parentObjs;

    try
    {
        auto method =
            _bus.new_method_call(type::ObjectMapperName, type::ObjectMapperPath,
                                 type::ObjectMapperName, "GetAncestors");

        method.append(isolateHardware.str);
        method.append(std::vector<std::string>({parentFruIfaceName._name}));

        auto reply = _bus.call(method);
        reply.read(parentObjs);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>(
            fmt::format("Exception [{}] to get the given object [{}] parent "
                        "by using the given parent interface [{}]",
                        e.what(), isolateHardware.str, parentFruIfaceName._name)
                .c_str());
        return std::nullopt;
    }

    if (parentObjs.empty())
    {
        log<level::ERR>(
            fmt::format("The given object [{}] does not contain any parent "
                        "with the given parent interface [{}]",
                        isolateHardware.str, parentFruIfaceName._name)
                .c_str());
        return std::nullopt;
    }
    else if (parentObjs.size() > 1)
    {
        // Should not happen, Always we will have one parent object with
        // the given parent interface for the given child object.
        log<level::ERR>(
            fmt::format("The given object [{}] contain more than one parent "
                        "with the given parent interface [{}]",
                        isolateHardware.str, parentFruIfaceName._name)
                .c_str());
        return std::nullopt;
    }

    return parentObjs.begin()->first;
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

        if (isolateHwInstanceInfo->first._name == "unit")
        {
            log<level::ERR>(
                fmt::format("Not allowed to isolate the given hardware [{}] "
                            "which is not modeled in BMC inventory",
                            isolateHardware.str)
                    .c_str());
            return std::nullopt;
        }

        auto isolateHwDetails = getIsotableHWDetailsByObjPath(isolateHardware);
        if (!isolateHwDetails.has_value())
        {
            log<level::ERR>(
                fmt::format("The given hardware inventory object [{}] "
                            "item interface is not found in isolatable "
                            "hardware list",
                            isolateHardware.str)
                    .c_str());
            return std::nullopt;
        }

        // Make sure the given isolateHardware invetory path is exist
        // getDBusServiceName() will throw exception if the given object
        // is not exist.
        utils::getDBusServiceName(_bus, isolateHardware.str,
                                  isolateHwDetails->first._interfaceName._name);

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
                isolateHwDetails->second._parentFruHwId._interfaceName);
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
        // FIXME: Use mapper to get dbus name instad of hardcode like below
        //        but, mapper failing when using "com.ibm.VPD" dbus tree.
        std::string dbusServiceName{"com.ibm.VPD.Manager"};

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
    std::string fruUnitDevTreePath{pdbg_target_path(devTreeTgt)};
    std::string fruUnitPdbgClass{pdbg_target_class_name(devTreeTgt)};

    struct pdbg_target* parentFruTarget = nullptr;
    if ((fruUnitPdbgClass == "ocmb") || (fruUnitPdbgClass == "mem_port"))
    {
        /**
         * FIXME: The assumption is, dimm is parent fru for "ocmb" and
         *        "mem_port" and each "ocmb" or "mem_port" will have one
         *        "dimm" so if something is changed then need to fix
         *        this logic.
         * @note  In phal cec device tree dimm is placed under ocmb->mem_port
         *        based on dimm pervasive path.
         */
        auto dimmCount = 0;
        struct pdbg_target* lastDimmTgt = nullptr;
        pdbg_for_each_target("dimm", devTreeTgt, lastDimmTgt)
        {
            parentFruTarget = lastDimmTgt;
            ++dimmCount;
        }

        if (dimmCount == 0)
        {
            log<level::ERR>(
                fmt::format("Failed to get the parent dimm target "
                            "from phal cec device tree for the given phal cec "
                            "device tree target [{}]",
                            fruUnitDevTreePath)
                    .c_str());
            return std::nullopt;
        }
        else if (dimmCount > 1)
        {
            log<level::ERR>(
                fmt::format("More [{}] dimm targets are present "
                            "in phal cec device tree for the given phal cec "
                            " device tree target [{}]",
                            dimmCount, fruUnitDevTreePath)
                    .c_str());
            return std::nullopt;
        }
    }
    else
    {
        /**
         * FIXME: Today, All FRU parts (units - both(chiplet and non-chiplet))
         *        are modelled under the respective processor in cec device
         *        tree so, if something changed then, need to revisit the
         *        logic which is used to get the FRU details of FRU unit.
         */
        parentFruTarget = pdbg_target_parent("proc", devTreeTgt);
        if (parentFruTarget == nullptr)
        {
            log<level::ERR>(
                fmt::format("Failed to get the processor target from phal cec "
                            "device tree for the given target [{}]",
                            fruUnitDevTreePath)
                    .c_str());
            return std::nullopt;
        }
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
            _bus, type::ObjectMapperPath, type::ObjectMapperName);

        auto method = _bus.new_method_call(
            dbusServiceName.c_str(), type::ObjectMapperPath,
            type::ObjectMapperName, "GetSubTreePaths");

        std::vector<std::string> listOfIfaces{interfaceName};

        method.append(parentObjPath.str, 0, listOfIfaces);

        auto resp = _bus.call(method);

        std::vector<std::string> recvPaths;
        resp.read(recvPaths);

        std::for_each(recvPaths.begin(), recvPaths.end(),
                      [&listOfChildsInventoryPath](const auto& ele) {
                          listOfChildsInventoryPath.push_back(
                              sdbusplus::message::object_path(ele));
                      });
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

        auto pdbgTgtClass{pdbg_target_class_name(*isolatedHwTgt)};
        if (pdbgTgtClass == nullptr)
        {
            log<level::ERR>(
                fmt::format("The given hardware [{}] pdbg target class "
                            "is missing, please make sure hardware unit "
                            "is added in pdbg ",
                            isolatedHwTgtDevTreePath)
                    .c_str());
            return std::nullopt;
        }
        std::string isolatedHwPdbgClass{pdbgTgtClass};

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
                [&isolateHw, &isolatedHwDetails, &isolatedHwInfo,
                 this](const auto& path) {
                    return isolatedHwDetails->second._invPathFuncLookUp(
                        path, isolateHw, isolatedHwInfo.first, this->_bus);
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
                [&parentFruHw, &parentFruHwDetails, &parentFruHwInfo,
                 this](const auto& path) {
                    return parentFruHwDetails->second._invPathFuncLookUp(
                        path, parentFruHw, parentFruHwInfo.first, this->_bus);
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

            InstanceId isolateHwInstId;
            // TODO Below decision need to be based on system core mode
            //     i.e whether need to use "fc" (in big core system) or
            //     "core" (in small core system) pdbg target class to get
            //     the appropriate target physical path from the phal
            //     cec device tree but, now using the "fc".
            if (isolatedHwPdbgClass == "core")
            {
                struct pdbg_target* parentFc =
                    pdbg_target_parent("fc", *isolatedHwTgt);
                if (parentFc == nullptr)
                {
                    log<level::ERR>(
                        fmt::format("Failed to get the parent FC target for "
                                    "the given device tree target path [{}]",
                                    isolatedHwTgtDevTreePath)
                            .c_str());
                    return std::nullopt;
                }
                isolateHwInstId = devtree::getHwInstIdFromDevTree(parentFc);
            }
            else
            {
                isolateHwInstId =
                    devtree::getHwInstIdFromDevTree(*isolatedHwTgt);
            }

            /**
             * If PrettyName is not empty then use that as instance name
             * because currently few isolatbale hardware subunits is not
             * modelled in BMC Inventory and Redfish so these subunits
             * need to look based on PrettyName to get inventory path.
             */
            std::string isolateHw;
            if (isolatedHwDetails->second._prettyName.empty())
            {
                isolateHw = isolatedHwDetails->first._itemObjectName._name +
                            (isolateHwInstId == 0xFFFFFFFF
                                 ? ""
                                 : std::to_string(isolateHwInstId));
            }
            else
            {
                isolateHw = isolatedHwDetails->second._prettyName;
            }

            auto isolateHwPath = std::find_if(
                childsInventoryPath->begin(), childsInventoryPath->end(),
                [&isolateHw, &isolatedHwDetails, &parentFruHwInfo,
                 this](const auto& path) {
                    return isolatedHwDetails->second._invPathFuncLookUp(
                        path, isolateHw, parentFruHwInfo.first, this->_bus);
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

IsItIsoHwInvPath itemInstance(const sdbusplus::message::object_path& objPath,
                              const std::string& instance,
                              const type::LocationCode& /* locCode */,
                              sdbusplus::bus::bus& /* bus */)
{
    return objPath.filename() == instance;
}

IsItIsoHwInvPath itemPrettyName(const sdbusplus::message::object_path& objPath,
                                const std::string& instance,
                                const type::LocationCode& /* locCode */,
                                sdbusplus::bus::bus& bus)
{
    try
    {
        auto prettyName = utils::getDBusPropertyVal<std::string>(
            bus, objPath, "xyz.openbmc_project.Inventory.Item", "PrettyName");
        return prettyName == instance;
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::WARNING>(fmt::format("Exception [{}] to get PrettyName for "
                                        "the given object path [{}]",
                                        e.what(), objPath.str)
                                .c_str());
        return false;
    }
}

IsItIsoHwInvPath
    itemLocationCode(const sdbusplus::message::object_path& objPath,
                     const std::string& /* instance */,
                     const type::LocationCode& locCode,
                     sdbusplus::bus::bus& bus)
{
    try
    {
        auto expandedLocCode = utils::getDBusPropertyVal<std::string>(
            bus, objPath,
            "xyz.openbmc_project.Inventory.Decorator.LocationCode",
            "LocationCode");

        auto unExpandedLocCode{devtree::getUnexpandedLocCode(expandedLocCode)};
        if (!unExpandedLocCode.has_value())
        {
            return false;
        }

        return *unExpandedLocCode == locCode;
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::WARNING>(fmt::format("Exception [{}] to get LocationCode "
                                        "for the given object path [{}]",
                                        e.what(), objPath.str)
                                .c_str());
        return false;
    }
}

} // namespace inv_path_lookup_func

} // namespace hw_isolation
