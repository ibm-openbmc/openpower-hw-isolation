// SPDX-License-Identifier: Apache-2.0

#include "openpower_guard_interface.hpp"

#include <fmt/format.h>

#include <libguard/guard_exception.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/HardwareIsolation/Errors/error.hpp>

namespace hw_isolation
{
namespace openpower_guard
{

using namespace phosphor::logging;
namespace FileError = sdbusplus::xyz::openbmc_project::Common::File::Error;
namespace CommonError = sdbusplus::xyz::openbmc_project::Common::Error;
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
        throw CommonError::InvalidArgument();
    }
    catch (libguard::exception::AlreadyGuarded& e)
    {
        throw HardwareIsolationError::IsolatedAlready();
    }
    catch (libguard::exception::GuardFileOverFlowed& e)
    {
        throw CommonError::NotAllowed();
    }
    return std::nullopt;
}

} // namespace openpower_guard
} // namespace hw_isolation
