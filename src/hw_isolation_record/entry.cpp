// SPDX-License-Identifier: Apache-2.0

#include "hw_isolation_record/entry.hpp"

#include "common/utils.hpp"
#include "hw_isolation_record/manager.hpp"

#include <fmt/format.h>

#include <phosphor-logging/elog-errors.hpp>

#include <ctime>

namespace hw_isolation
{
namespace record
{
namespace entry
{

using namespace phosphor::logging;

Entry::Entry(sdbusplus::bus::bus& bus, const std::string& objPath,
             hw_isolation::record::Manager& hwIsolationRecordMgr,
             const EntryId entryId, const EntryRecordId entryRecordId,
             const EntrySeverity isolatedHwSeverity,
             const EntryResolved entryIsResolved,
             const type::AssociationDef& associationDef,
             const openpower_guard::EntityPath& entityPath) :
    type::ServerObject<EntryInterface, AssociationDefInterface, EpochTime,
                       DeleteInterface>(
        bus, objPath.c_str(),
        type::ServerObject<EntryInterface, AssociationDefInterface, EpochTime,
                           DeleteInterface>::action::defer_emit),
    _bus(bus), _hwIsolationRecordMgr(hwIsolationRecordMgr), _entryId(entryId),
    _entryRecordId(entryRecordId), _entityPath(entityPath)
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

void Entry::resolveEntry(bool clearRecord)
{
    if (!resolved())
    {
        if (clearRecord)
        {
            openpower_guard::clear(_entryRecordId);
        }
        resolved(true);
        for (auto& assoc : associations())
        {
            if (std::get<0>(assoc) == "isolated_hw")
            {
                hw_isolation::utils::setEnabledProperty(
                    _bus, std::get<2>(assoc), true);
                break;
            }
        }

        _hwIsolationRecordMgr.eraseEntry(_entryId);
    }
}

void Entry::delete_()
{
    // throws exception if not allowed
    hw_isolation::utils::isHwDeisolationAllowed(_bus);

    // throws exception if the user tried deisolate the system
    // isolated hardware entry
    if (severity() != EntrySeverity::Manual)
    {
        log<level::ERR>(fmt::format("User is not allowed to clear the system "
                                    "isolated hardware entry")
                            .c_str());
        throw type::CommonError::InsufficientPermission();
    }

    resolveEntry();
}

openpower_guard::EntityPath Entry::getEntityPath() const
{
    return _entityPath;
}

EntryRecordId Entry::getEntryRecId() const
{
    return _entryRecordId;
}

namespace utils
{

std::optional<EntrySeverity>
    getEntrySeverityType(const openpower_guard::GardType gardType)
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
            return std::nullopt;
    }
}

std::optional<openpower_guard::GardType>
    getGuardType(const EntrySeverity severity)
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
            return std::nullopt;
    }
}

} // namespace utils
} // namespace entry
} // namespace record
} // namespace hw_isolation
