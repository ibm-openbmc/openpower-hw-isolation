// SPDX-License-Identifier: Apache-2.0

#include "hardware_isolation_entry.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>

#include <ctime>

namespace hw_isolation
{
namespace entry
{

using namespace phosphor::logging;

Entry::Entry(sdbusplus::bus::bus& bus, const std::string& objPath,
             const EntryId entryId, const EntryRecordId entryRecordId,
             const EntrySeverity isolatedHwSeverity,
             const EntryResolved entryIsResolved,
             const AssociationDef& associationDef) :
    type::ServerObject<EntryInterface, AssociationDefInterface, EpochTime>(
        bus, objPath.c_str(), true),
    _entryId(entryId), _entryRecordId(entryRecordId)
{
    // Setting properties which are defined in EntryInterface
    severity(isolatedHwSeverity);
    resolved(entryIsResolved);
    associations(associationDef);

    // Set creation time of isolated hardware entry
    std::time_t timeStamp = std::time(nullptr);
    elapsed(timeStamp);

    // Emit the signal for entry object creation since it deferred in
    // interface constructor
    this->emit_object_added();
}

namespace utils
{

EntrySeverity getEntrySeverityType(const openpower_guard::GardType gardType)
{
    switch (gardType)
    {
        case openpower_guard::GardType::GARD_Unrecoverable:
        case openpower_guard::GardType::GARD_Fatal:
            return EntrySeverity::Critical;

        case openpower_guard::GardType::GARD_User_Manual:
            return EntrySeverity::Manual;

        case openpower_guard::GardType::GARD_Predictive:
            return EntrySeverity::Warning;

        default:
            log<level::ERR>(
                fmt::format("Unsupported GardType [{}] was given ",
                            "to get the hardware isolation entry severity type",
                            gardType)
                    .c_str());
            return EntrySeverity::Warning;
    }
}

openpower_guard::GardType getGuardType(const EntrySeverity severity)
{
    switch (severity)
    {
        case EntrySeverity::Critical:
            return openpower_guard::GardType::GARD_Fatal;
        case EntrySeverity::Manual:
            return openpower_guard::GardType::GARD_User_Manual;
        case EntrySeverity::Warning:
            return openpower_guard::GardType::GARD_Predictive;

        default:
            log<level::ERR>(
                fmt::format("Unsupported EntrySeverity [{}] "
                            "was given to the get openpower gard type",
                            EntryInterface::convertTypeToString(severity))
                    .c_str());
            return openpower_guard::GardType::GARD_Predictive;
    }
}

} // namespace utils
} // namespace entry
} // namespace hw_isolation
