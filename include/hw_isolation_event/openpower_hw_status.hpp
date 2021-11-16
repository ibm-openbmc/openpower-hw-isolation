// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hw_isolation_event/event.hpp"

namespace hw_isolation
{
namespace event
{
namespace openpower_hw_status
{

/**
 * - Enums to indicate non-error reason for a hardwares deconfiguration.
 * - These enuems used in HwasSate.deconfiguredByEid.
 * - The enums list is picked from OpenPOWER/Hostboot component.
 *   (src/include/usr/hwas/common/deconfigGard.H)
 */
enum DeconfiguredByReason
{
    INVALID_DECONFIGURED_BY_REASON,

    DECONFIGURED_BY_CODE_BASE = 0x0000FF00,

    DECONFIGURED_BY_MANUAL_GARD,         // BASE | 0x01
    DECONFIGURED_BY_FIELD_CORE_OVERRIDE, // BASE | 0x02
    DECONFIGURED_BY_MEMORY_CONFIG,       // BASE | 0x03

    DECONFIGURED_BY_NO_CHILD_MCA,     // BASE | 0x04
    DECONFIGURED_BY_NO_CHILD_MEMBUF = // Deprecated
    DECONFIGURED_BY_NO_CHILD_MCA,
    DECONFIGURED_BY_NO_CHILD_MEMBUF_OR_MCA = // Deprecated
    DECONFIGURED_BY_NO_CHILD_MCA,

    DECONFIGURED_BY_BUS_DECONFIG,     // BASE | 0x05
    DECONFIGURED_BY_PRD,              // BASE | 0x06
    DECONFIGURED_BY_PHYP,             // BASE | 0x07
    DECONFIGURED_BY_SPCN,             // BASE | 0x08
    DECONFIGURED_BY_NO_PARENT_MEMBUF, // BASE | 0x09
    DECONFIGURED_BY_NO_CHILD_DIMM,    // BASE | 0x0A

    DECONFIGURED_BY_NO_PARENT_DMI,  // BASE | 0x0B
    DECONFIGURED_BY_NO_PARENT_MCS = // Deprecated
    DECONFIGURED_BY_NO_PARENT_DMI,

    DECONFIGURED_BY_NO_CHILD_MBA, // BASE | 0x0C

    DECONFIGURED_BY_NO_PARENT_MBA_OR_MCA, // BASE | 0x0D
    DECONFIGURED_BY_NO_PARENT_MBA =       // Deprecated
    DECONFIGURED_BY_NO_PARENT_MBA_OR_MCA,

    CONFIGURED_BY_RESOURCE_RECOVERY, // BASE | 0x0E

    DECONFIGURED_BY_EQ_DECONFIG,          // BASE | 0x0F
    DECONFIGURED_BY_FC_DECONFIG,          // BASE | 0x10
    DECONFIGURED_BY_CORE_DECONFIG,        // BASE | 0x11
    DECONFIGURED_BY_PHB_DECONFIG,         // BASE | 0x12
    DECONFIGURED_BY_PEC_DECONFIG,         // BASE | 0x13
    DECONFIGURED_BY_NO_CHILD_MCS,         // BASE | 0x14
    DECONFIGURED_BY_NO_PARENT_MCBIST,     // BASE | 0x15
    DECONFIGURED_BY_DISABLED_PORT,        // BASE | 0x16
    DECONFIGURED_BY_NO_CHILD_MI,          // BASE | 0x17
    DECONFIGURED_BY_NO_CHILD_DMI,         // BASE | 0x18
    DECONFIGURED_BY_NO_PARENT_MC,         // BASE | 0x19
    DECONFIGURED_BY_NO_PARENT_MI,         // BASE | 0x1A
    DECONFIGURED_BY_NO_MATCHING_LINK_SET, // BASE | 0x1B
    DECONFIGURED_BY_OBUS_MODE,            // BASE | 0x1C
    DECONFIGURED_BY_NO_CHILD_OMI,         // BASE | 0x1D
    DECONFIGURED_BY_NO_PARENT_MCC,        // BASE | 0x1E
    DECONFIGURED_BY_NO_CHILD_MEM_PORT,    // BASE | 0x1F
    DECONFIGURED_BY_NO_PARENT_OMI,        // BASE | 0x20
    DECONFIGURED_BY_NO_CHILD_OCMB_CHIP,   // BASE | 0x21
    DECONFIGURED_BY_NO_PARENT_OCMB_CHIP,  // BASE | 0x22
    DECONFIGURED_BY_NO_PARENT_OMIC,       // BASE | 0x23
    DECONFIGURED_BY_INACTIVE_PAU,         // BASE | 0x24
    DECONFIGURED_BY_NO_CHILD_OMIC,        // BASE | 0x25
    DECONFIGURED_BY_NO_CHILD_MCC,         // BASE | 0x26
    DECONFIGURED_BY_NO_PARENT_MEM_PORT,   // BASE | 0x27
    DECONFIGURED_BY_NO_PARENT_PAUC,       // BASE | 0x28
    DECONFIGURED_BY_NO_CHILD_PMIC,        // BASE | 0x29
    DECONFIGURED_BY_NO_PEER_TARGET,       // BASE | 0x2A

    // mask - these bits mean it's a PLID/EID and not an enum
    //        for the deconfigured hardwares
    DECONFIGURED_BY_PLID_MASK = 0xFFFF0000,
};

} // namespace openpower_hw_status
} // namespace event
} // namespace hw_isolation
