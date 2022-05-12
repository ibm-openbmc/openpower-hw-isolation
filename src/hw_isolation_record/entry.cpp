// SPDX-License-Identifier: Apache-2.0

#include "hw_isolation_record/entry.hpp"

#include "common/utils.hpp"
#include "hw_isolation_record/manager.hpp"

#include <fmt/format.h>

#include <cereal/archives/binary.hpp>
#include <phosphor-logging/elog-errors.hpp>

#include <ctime>
#include <fstream>

// Associate Entry Class with version number
constexpr uint32_t Cereal_EntryClassVersion = 1;
CEREAL_CLASS_VERSION(hw_isolation::record::entry::Entry,
                     Cereal_EntryClassVersion);

namespace hw_isolation
{
namespace record
{
namespace entry
{
namespace fs = std::filesystem;

using namespace phosphor::logging;

Entry::Entry(sdbusplus::bus::bus& bus, const std::string& objPath,
             hw_isolation::record::Manager& hwIsolationRecordMgr,
             const EntryRecordId entryRecordId,
             const EntrySeverity isolatedHwSeverity,
             const EntryResolved entryIsResolved,
             const type::AssociationDef& associationDef,
             const openpower_guard::EntityPath& entityPath) :
    type::ServerObject<EntryInterface, AssociationDefInterface, EpochTime,
                       DeleteInterface>(
        bus, objPath.c_str(),
        type::ServerObject<EntryInterface, AssociationDefInterface, EpochTime,
                           DeleteInterface>::action::defer_emit),
    _bus(bus), _hwIsolationRecordMgr(hwIsolationRecordMgr),
    _entryRecordId(entryRecordId), _entityPath(entityPath)
{
    // Setting properties which are defined in EntryInterface
    severity(isolatedHwSeverity);
    resolved(entryIsResolved);
    associations(associationDef);

    // Set creation time of isolated hardware entry
    std::time_t timeStamp = std::time(nullptr);
    elapsed(timeStamp);

    // Signal won't be sent since D-Bus name requested after restore
    // even if we sent noop at the mapper side.
    if (!deserialize())
    {
        // Need to serialize entry members if it did not deserialize
        // since this constructor might be called in the restore path
        // and at the runtime
        serialize();
    }

    // Emit the signal for entry object creation since it deferred in
    // interface constructor
    this->emit_object_added();
}

Entry::~Entry()
{
    fs::path path{fmt::format(HW_ISOLATION_ENTRY_PERSIST_PATH, _entryRecordId)};
    if (fs::exists(path))
    {
        fs::remove(path);
    }
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

        _hwIsolationRecordMgr.eraseEntry(_entryRecordId);
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

void Entry::serialize()
{
    fs::path path{fmt::format(HW_ISOLATION_ENTRY_PERSIST_PATH, _entryRecordId)};
    try
    {
        std::ofstream os(path.c_str(), std::ios::binary);
        cereal::BinaryOutputArchive oarchive(os);
        oarchive(*this);
    }
    catch (const cereal::Exception& e)
    {
        log<level::ERR>(fmt::format("Exception: [{}] during serialize the "
                                    "hardware isolation entry into {}",
                                    e.what(), path.string())
                            .c_str());
        fs::remove(path);
    }
}

bool Entry::deserialize()
{
    fs::path path{fmt::format(HW_ISOLATION_ENTRY_PERSIST_PATH, _entryRecordId)};
    try
    {
        if (fs::exists(path))
        {
            std::ifstream is(path.c_str(), std::ios::in | std::ios::binary);
            cereal::BinaryInputArchive iarchive(is);
            iarchive(*this);
            return true;
        }
        return false;
    }
    catch (const cereal::Exception& e)
    {
        log<level::ERR>(fmt::format("Exception: [{}] during deserialize the "
                                    "hardware isolation entry from {}",
                                    e.what(), path.string())
                            .c_str());
        fs::remove(path);
        return false;
    }
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
