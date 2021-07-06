// SPDX-License-Identifier: Apache-2.0

#include "common_types.hpp"
#include "openpower_guard_interface.hpp"

#include <fmt/format.h>

#include <libguard/guard_exception.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/HardwareIsolation/Errors/error.hpp>

namespace hw_isolation
{
namespace openpower_guard
{

using namespace phosphor::logging;
namespace FileError = sdbusplus::xyz::openbmc_project::Common::File::Error;
namespace HardwareIsolationError =
    sdbusplus::xyz::openbmc_project::HardwareIsolation::Errors::Error;

std::optional<GuardRecord> create(const EntityPath& entityPath,
                                  const uint32_t errorLogId,
                                  const GardType guardType)
{
    try
    {
        return libguard::create(entityPath, errorLogId, guardType);
    }
    catch (libguard::exception::GuardFileOpenFailed& e)
    {
        throw FileError::Open();
    }
    catch (libguard::exception::GuardFileReadFailed& e)
    {
        throw FileError::Read();
    }
    catch (libguard::exception::GuardFileWriteFailed& e)
    {
        throw FileError::Write();
    }
    catch (libguard::exception::InvalidEntityPath& e)
    {
        throw type::CommonError::InvalidArgument();
    }
    catch (libguard::exception::AlreadyGuarded& e)
    {
        throw HardwareIsolationError::IsolatedAlready();
    }
    catch (libguard::exception::GuardFileOverFlowed& e)
    {
        throw type::CommonError::NotAllowed();
    }
    return std::nullopt;
}

void clear(const uint32_t recordId)
{
    try
    {
        libguard::clear(recordId);
    }
    catch (libguard::exception::GuardFileOpenFailed& e)
    {
        throw FileError::Open();
    }
    catch (libguard::exception::GuardFileReadFailed& e)
    {
        throw FileError::Read();
    }
    catch (libguard::exception::GuardFileWriteFailed& e)
    {
        throw FileError::Write();
    }
    catch (libguard::exception::InvalidEntityPath& e)
    {
        throw type::CommonError::InvalidArgument();
    }
    catch (libguard::exception::AlreadyGuarded& e)
    {
        throw HardwareIsolationError::IsolatedAlready();
    }
    catch (libguard::exception::GuardFileOverFlowed& e)
    {
        throw type::CommonError::NotAllowed();
    }
}

GuardRecords getAll()
{
    GuardRecords records;
    try
    {
        records = libguard::getAll();
    }
    catch (libguard::exception::GuardFileOpenFailed& e)
    {
        throw FileError::Open();
    }
    catch (libguard::exception::GuardFileReadFailed& e)
    {
        throw FileError::Read();
    }
    catch (libguard::exception::GuardFileWriteFailed& e)
    {
        throw FileError::Write();
    }
    catch (libguard::exception::InvalidEntityPath& e)
    {
        throw type::CommonError::InvalidArgument();
    }
    catch (libguard::exception::AlreadyGuarded& e)
    {
        throw HardwareIsolationError::IsolatedAlready();
    }
    catch (libguard::exception::GuardFileOverFlowed& e)
    {
        throw type::CommonError::NotAllowed();
    }

    return records;
}

} // namespace openpower_guard
} // namespace hw_isolation
