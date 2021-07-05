// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common_types.hpp"
#include "phal_devtree_utils.hpp"

#include <map>
#include <optional>

namespace hw_isolation
{
namespace isolatable_hws
{

using namespace hw_isolation::type;

/**
 * @class IsolatableHWs
 *
 * @brief This class is used to maintain an isolatable hardware list
 *        and it contains helper members and functions to get
 *        isolatable hardware details.
 */
class IsolatableHWs
{
  public:
    IsolatableHWs(const IsolatableHWs&) = delete;
    IsolatableHWs& operator=(const IsolatableHWs&) = delete;
    IsolatableHWs(IsolatableHWs&&) = delete;
    IsolatableHWs& operator=(IsolatableHWs&&) = delete;
    ~IsolatableHWs() = default;

    /**
     * @brief Constructor to initialize isolatable hardware list
     */
    IsolatableHWs();

    /**
     * @brief HW_Details used to hold the required hardware
     *        details that can be used to isolate.
     */
    struct HW_Details
    {
        /**
         * @brief HwId used to hold the id which can be used
         *        to get the hardware lookup path from phal
         *        cec device tree or bmc inventory to isolate.
         */
        struct HwId
        {
            /**
             * @brief ItemInterfaceName used to hold bmc inventory
             *        item (hardware) interface name that can be
             *        used to get inventory object path.
             */
            struct ItemInterfaceName
            {
                std::string _name;
                explicit ItemInterfaceName(const std::string& ifaceName) :
                    _name(ifaceName)
                {}
            };

            /**
             * @brief ItemObjectName used to hold bmc inventory
             *        item (hardware) object name that can be
             *        used to get inventory object path.
             *        This is helpful when getting inventory
             *        item dbus object path alone to isolate.
             */
            struct ItemObjectName
            {
                std::string _name;
                explicit ItemObjectName(const std::string& objName) :
                    _name(objName)
                {}
            };

            /**
             * @brief PhalPdbgClassName used to hold phal pdbg
             *        class name (which is actually available for
             *        hardwares which are present in phal cec device tree)
             *        that can be used to get the isolatable hardware
             *        node path from phal cec device tree.
             */
            struct PhalPdbgClassName
            {
                std::string _name;
                explicit PhalPdbgClassName(const std::string& pClassName) :
                    _name(pClassName)
                {}
            };

            ItemInterfaceName _interfaceName;
            ItemObjectName _itemObjectName;
            PhalPdbgClassName _pdbgClassName;

            HwId() = delete;

            HwId(ItemInterfaceName ifaceName, ItemObjectName oName,
                 PhalPdbgClassName pClassName) :
                _interfaceName(ifaceName),
                _itemObjectName(oName), _pdbgClassName(pClassName)
            {}

            HwId(const std::string& ifaceName, const std::string& oName,
                 const std::string& pClassName) :
                _interfaceName(ItemInterfaceName(ifaceName)),
                _itemObjectName(ItemObjectName(oName)),
                _pdbgClassName(PhalPdbgClassName(pClassName))
            {}

            explicit HwId(ItemInterfaceName ifaceName) :
                _interfaceName(ifaceName), _itemObjectName(""),
                _pdbgClassName("")
            {}

            explicit HwId(ItemObjectName oName) :
                _interfaceName(""), _itemObjectName(oName), _pdbgClassName("")
            {}

            explicit HwId(PhalPdbgClassName pClassName) :
                _interfaceName(""), _itemObjectName(""),
                _pdbgClassName(pClassName)
            {}

            /**
             * @brief A == operator that will match on any Name
             *        being equal if the other names are empty, so that
             *        one can look up a HwId with just one of the Name.
             */
            bool operator==(const HwId& hwId) const
            {
                if (!hwId._interfaceName._name.empty())
                {
                    return hwId._interfaceName._name == _interfaceName._name;
                }

                if (!hwId._itemObjectName._name.empty())
                {
                    return hwId._itemObjectName._name == _itemObjectName._name;
                }

                if (!hwId._pdbgClassName._name.empty())
                {
                    return hwId._pdbgClassName._name == _pdbgClassName._name;
                }

                return false;
            }

            /**
             * @brief A < operator is required to stl_function.h to insert
             *        in STL based on some order.
             *
             *        Using PhalPdbgClassName since most of lookup will happen
             *        by using PhalPdbgClassName key.
             */
            bool operator<(const HwId& hwId) const
            {
                return hwId._pdbgClassName._name < _pdbgClassName._name;
            }
        };

        bool _isItFRU;
        HwId _parentFruHwId;
        devtree::lookup_func::LookupFuncForPhysPath _physPathFuncLookUp;

        HW_Details(
            bool isItFRU, const HwId& parentFruHwId,
            devtree::lookup_func::LookupFuncForPhysPath physPathFuncLookUp) :
            _isItFRU(isItFRU),
            _parentFruHwId(parentFruHwId),
            _physPathFuncLookUp(physPathFuncLookUp)
        {}
    };

  private:
    /**
     * @brief The list of isolatable hardwares
     */
    std::map<HW_Details::HwId, HW_Details> _isolatableHWsList;

    /**
     * @brief Helper function to segregate the instance name and id
     *        from given hardware dbus object name.
     *        Example: core0 -> std::pair<core, 0>
     *
     * @param[in] objName - hardware dbus object name to segregate into
     *                      instance name and id
     *
     * @return pair<InstanceName, InstanceId> on success or
     *         empty optional on failure
     */
    std::optional<
        std::pair<IsolatableHWs::HW_Details::HwId::ItemObjectName, InstanceId>>
        getInstanceInfo(const std::string& objName) const;

    /**
     * @brief Get the HwID based on given ItemInterfaceName or ItemObjectName
     *        or PhalPdbgClassName.
     *
     * @param[in] id - The ID to find the HwID.
     *
     * @return the hardware details for the given HwID
     *         or an empty optional if not found.
     */
    std::optional<std::pair<HW_Details::HwId, HW_Details>>
        getIsotableHWDetails(const HW_Details::HwId& id) const;

};

} // namespace isolatable_hws
} // namespace hw_isolation
