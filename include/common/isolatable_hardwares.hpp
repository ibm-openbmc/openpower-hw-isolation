// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common_types.hpp"
#include "phal_devtree_utils.hpp"

#include <map>
#include <optional>

namespace hw_isolation
{

/**
 * @brief Used to add functions that will use to get to know whether the
 *        given inventory object path is isolated hardware inventory path
 *        (maybe its parent) or not.
 */
namespace inv_path_lookup_func
{
using IsItIsoHwInvPath = bool;

/**
 * @brief Lookup function signature
 *
 *
 * @param[in] bus - the attached bus
 * @param[in] object_path - the inventory object path to check whether the
 *                          isolated hardware inventory object path or not
 * @param[in] variant - the isolated hardware id
 *
 * @return IsItIsoHwInvPath to indicate whether the given inventory object
 *         path is isolated hardware inventory path (maybe its parent) or not.
 *
 * @note All lookup functions which are added in this namespace should
 *       match with below signature.
 */
// type::LocationCode and PrettyName are string type.
using UniqueHwId = std::variant<type::InstanceId, std::string>;

using LookupFuncForInvPath = std::function<IsItIsoHwInvPath(
    sdbusplus::bus::bus&, const sdbusplus::message::object_path&,
    const UniqueHwId&)>;

IsItIsoHwInvPath itemInstanceId(sdbusplus::bus::bus& bus,
                                const sdbusplus::message::object_path& objPath,
                                const UniqueHwId& instanceId);

IsItIsoHwInvPath itemPrettyName(sdbusplus::bus::bus& bus,
                                const sdbusplus::message::object_path& objPath,
                                const UniqueHwId& prettyName);

IsItIsoHwInvPath
    itemLocationCode(sdbusplus::bus::bus& bus,
                     const sdbusplus::message::object_path& objPath,
                     const UniqueHwId& locCode);

} // namespace inv_path_lookup_func

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
    IsolatableHWs(sdbusplus::bus::bus& bus);

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
            PhalPdbgClassName _pdbgClassName;

            HwId() = delete;

            HwId(ItemInterfaceName ifaceName, PhalPdbgClassName pClassName) :
                _interfaceName(ifaceName), _pdbgClassName(pClassName)
            {}

            HwId(const std::string& ifaceName, const std::string& pClassName) :
                _interfaceName(ItemInterfaceName(ifaceName)),
                _pdbgClassName(PhalPdbgClassName(pClassName))
            {}

            explicit HwId(ItemInterfaceName ifaceName) :
                _interfaceName(ifaceName), _pdbgClassName("")
            {}

            explicit HwId(PhalPdbgClassName pClassName) :
                _interfaceName(""), _pdbgClassName(pClassName)
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
        inv_path_lookup_func::LookupFuncForInvPath _invPathFuncLookUp;
        std::string _prettyName;

        HW_Details(
            bool isItFRU, const HwId& parentFruHwId,
            devtree::lookup_func::LookupFuncForPhysPath physPathFuncLookUp,
            inv_path_lookup_func::LookupFuncForInvPath invPathFuncLookUp,
            const std::string& prettyName) :
            _isItFRU(isItFRU),
            _parentFruHwId(parentFruHwId),
            _physPathFuncLookUp(physPathFuncLookUp),
            _invPathFuncLookUp(invPathFuncLookUp), _prettyName(prettyName)
        {}
    };

    /**
     * @brief Used to get physical path of isolating hardware
     *
     * @param[in] isolateHardware - isolate hardware dbus object path
     *
     * @return Physical path of isolating hardware on success
     *         Empty optional on failure
     */
    std::optional<devtree::DevTreePhysPath>
        getPhysicalPath(const sdbusplus::message::object_path& isolateHardware);

    /**
     * @brief Used to get the inventory path of isolated hardware
     *
     * @param[in] physicalPath - The physical path of isolated hardware
     *
     * @return The isolated hardware inventory path on success
     *         Empty optional on failure
     */
    std::optional<sdbusplus::message::object_path>
        getInventoryPath(const devtree::DevTreePhysPath& physicalPath);

  private:
    /**
     * @brief Attached bus connection
     */
    sdbusplus::bus::bus& _bus;

    /**
     * @brief The list of isolatable hardwares
     */
    std::map<HW_Details::HwId, HW_Details> _isolatableHWsList;

    /**
     * @brief Get the HwID based on given ItemInterfaceName or
     *        PhalPdbgClassName.
     *
     * @param[in] id - The ID to find the HwID.
     *
     * @return the hardware details for the given HwID
     *         or an empty optional if not found.
     */
    std::optional<std::pair<HW_Details::HwId, HW_Details>>
        getIsotableHWDetails(const HW_Details::HwId& id) const;

    /**
     * @brief Get the HwID based on the given D-Bus object path.
     *
     * @param[in] dbusObjPath - The D-Bus object to get HwID.
     *
     * @return the hardware details for the given D-Bus object path
     *         or an empty optional if not found.
     */
    std::optional<std::pair<HW_Details::HwId, HW_Details>>
        getIsotableHWDetailsByObjPath(
            const sdbusplus::message::object_path& dbusObjPath) const;

    /**
     * @brief Used to get location code from given dbus object path
     *
     * @param[in] dbusObjPath - Dbus object path to get location code
     *
     * @return LocationCode on success
     *         Throw exception on failure
     */
    LocationCode
        getLocationCode(const sdbusplus::message::object_path& dbusObjPath);

    /**
     * @brief Used to get parent fru dbus object path
     *        by using isolate hardware dbus object path
     *
     * @param[in] isolateHardware - isolate hardware dbus object path
     * @param[in] parentFruIfaceName - isolate hardware parent fru interface
     *
     * @return Parent FRU dbus object path on success
     *         or an empty optional if not found.
     */
    std::optional<sdbusplus::message::object_path> getParentFruObjPath(
        const sdbusplus::message::object_path& isolateHardware,
        const IsolatableHWs::HW_Details::HwId::ItemInterfaceName&
            parentFruIfaceName) const;

    /**
     * @brief Used to get the list of inventory object path by using
     *        given unexpanded location code
     *
     * @param[in] unexpandedLocCode - The unexpanded location code to get
     *                                the list of inventory object path
     *
     * @return The list of inventory object path on success
     *         Empty optional on failure
     */
    std::optional<std::vector<sdbusplus::message::object_path>>
        getInventoryPathsByLocCode(const LocationCode& unexpandedLocCode);

    /**
     * @brief Used to get the parent fru phal cec device tree target
     *        by using isolated phal cec device tree target
     *
     * @param[in] devTreeTgt - The phal cec device tree target to get
     *                         the parent fru target
     *
     * @return The parent fru device tree target on success
     *         Empty optional on failure
     */
    std::optional<struct pdbg_target*>
        getParentFruPhalDevTreeTgt(struct pdbg_target* devTreeTgt);

    /**
     * @brief Used to get child inventory path by using parent
     *        parent inventory path with specific interface
     *
     * @param[in] parentObjPath - The parent object path to get subtrees
     * @param[in] interfaceName - The child interface name
     *
     * @return The list of child inventory path on success
     *         Empty optional on failure
     */
    std::optional<std::vector<sdbusplus::message::object_path>>
        getChildsInventoryPath(
            const sdbusplus::message::object_path& parentObjPath,
            const std::string& interfaceName);
};

} // namespace isolatable_hws
} // namespace hw_isolation
