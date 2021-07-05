// SPDX-License-Identifier: Apache-2.0

#include "isolatable_hardwares.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>

namespace hw_isolation
{
namespace isolatable_hws
{

using namespace phosphor::logging;

IsolatableHWs::IsolatableHWs()
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

} // namespace isolatable_hws
} // namespace hw_isolation
