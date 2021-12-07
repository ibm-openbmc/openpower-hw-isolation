// SPDX-License-Identifier: Apache-2.0

#include "hw_isolation_event/openpower_hw_status.hpp"

#include "common/error_log.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>

namespace hw_isolation
{
namespace event
{
namespace openpower_hw_status
{

using namespace phosphor::logging;

std::pair<event::EventMsg, event::EventSeverity>
    convertDeconfiguredByReasonFromEnum(const DeconfiguredByReason& reason)
{
    switch (reason)
    {
        case DeconfiguredByReason::INVALID_DECONFIGURED_BY_REASON:
            return std::make_pair("Invalid", event::EventSeverity::Warning);
        case DeconfiguredByReason::DECONFIGURED_BY_FIELD_CORE_OVERRIDE:
            return std::make_pair("FCO-Deconfigured", event::EventSeverity::Ok);
        case DeconfiguredByReason::CONFIGURED_BY_RESOURCE_RECOVERY:
            return std::make_pair("Recovered", event::EventSeverity::Warning);
        case DeconfiguredByReason::DECONFIGURED_BY_MANUAL_GARD:
        case DeconfiguredByReason::DECONFIGURED_BY_MEMORY_CONFIG:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_CHILD_MCA:
        case DeconfiguredByReason::DECONFIGURED_BY_BUS_DECONFIG:
        case DeconfiguredByReason::DECONFIGURED_BY_PRD:
        case DeconfiguredByReason::DECONFIGURED_BY_SPCN:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_PARENT_MEMBUF:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_CHILD_DIMM:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_PARENT_DMI:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_CHILD_MBA:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_PARENT_MBA_OR_MCA:
        case DeconfiguredByReason::DECONFIGURED_BY_EQ_DECONFIG:
        case DeconfiguredByReason::DECONFIGURED_BY_FC_DECONFIG:
        case DeconfiguredByReason::DECONFIGURED_BY_CORE_DECONFIG:
        case DeconfiguredByReason::DECONFIGURED_BY_PHB_DECONFIG:
        case DeconfiguredByReason::DECONFIGURED_BY_PEC_DECONFIG:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_CHILD_MCS:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_PARENT_MCBIST:
        case DeconfiguredByReason::DECONFIGURED_BY_DISABLED_PORT:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_CHILD_MI:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_CHILD_DMI:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_PARENT_MC:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_PARENT_MI:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_MATCHING_LINK_SET:
        case DeconfiguredByReason::DECONFIGURED_BY_OBUS_MODE:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_CHILD_OMI:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_PARENT_MCC:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_CHILD_MEM_PORT:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_PARENT_OMI:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_CHILD_OCMB_CHIP:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_PARENT_OCMB_CHIP:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_PARENT_OMIC:
        case DeconfiguredByReason::DECONFIGURED_BY_INACTIVE_PAU:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_CHILD_OMIC:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_CHILD_MCC:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_PARENT_MEM_PORT:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_PARENT_PAUC:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_CHILD_PMIC:
        case DeconfiguredByReason::DECONFIGURED_BY_NO_PEER_TARGET:
            return std::make_pair("By Association",
                                  event::EventSeverity::Warning);

        default:
            log<level::ERR>(
                fmt::format("Unsupported deconfigured reason is given [{}]",
                            reason)
                    .c_str());
            error_log::createErrorLog(error_log::HwIsolationGenericErrMsg,
                                      error_log::Level::Informational,
                                      error_log::CollectTraces);
            return std::make_pair("Unknown", event::EventSeverity::Warning);
    }
}

} // namespace openpower_hw_status
} // namespace event
} // namespace hw_isolation
