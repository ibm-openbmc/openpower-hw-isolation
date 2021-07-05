// SPDX-License-Identifier: Apache-2.0

#include "isolatable_hardwares.hpp"

namespace hw_isolation
{
namespace isolatable_hws
{

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

} // namespace isolatable_hws
} // namespace hw_isolation
