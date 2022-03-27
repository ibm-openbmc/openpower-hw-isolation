// SPDX-License-Identifier: Apache-2.0

#include "hw_isolation_record/openpower_guard_interface.hpp"

#include "common/common_types.hpp"

#include <fmt/format.h>

#include <libguard/guard_exception.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/HardwareIsolation/error.hpp>

namespace hw_isolation
{
namespace openpower_guard
{

using namespace phosphor::logging;
namespace FileError = sdbusplus::xyz::openbmc_project::Common::File::Error;
namespace HardwareIsolationError =
    sdbusplus::xyz::openbmc_project::HardwareIsolation::Error;

/**
 * @brief Helper MACRO to call libguard interface and to have
 *        common exception handler
 *
 * @param[in] INTERFACE_SIGNATURE - To replace inside try block
 */
#define CALL_LIBGUARD_INTERFACE(INTERFACE_SIGNATURE)                           \
    try                                                                        \
    {                                                                          \
        INTERFACE_SIGNATURE                                                    \
    }                                                                          \
    catch (libguard::exception::GuardFileOpenFailed & e)                       \
    {                                                                          \
        throw FileError::Open();                                               \
    }                                                                          \
    catch (libguard::exception::GuardFileReadFailed & e)                       \
    {                                                                          \
        throw FileError::Read();                                               \
    }                                                                          \
    catch (libguard::exception::GuardFileWriteFailed & e)                      \
    {                                                                          \
        throw FileError::Write();                                              \
    }                                                                          \
    catch (libguard::exception::GuardFileSeekFailed & e)                       \
    {                                                                          \
        throw FileError::Seek();                                               \
    }                                                                          \
    catch (libguard::exception::InvalidEntityPath & e)                         \
    {                                                                          \
        throw type::CommonError::InvalidArgument();                            \
    }                                                                          \
    catch (libguard::exception::AlreadyGuarded & e)                            \
    {                                                                          \
        throw HardwareIsolationError::IsolatedAlready();                       \
    }                                                                          \
    catch (libguard::exception::GuardFileOverFlowed & e)                       \
    {                                                                          \
        throw type::CommonError::TooManyResources();                           \
    }

std::optional<GuardRecord> create(const EntityPath& entityPath,
                                  const uint32_t errorLogId,
                                  const GardType guardType)
{
    CALL_LIBGUARD_INTERFACE(
        return libguard::create(entityPath, errorLogId, guardType);)

    return std::nullopt;
}

void clear(const uint32_t recordId)
{
    CALL_LIBGUARD_INTERFACE(libguard::clear(recordId);)
}

GuardRecords getAll(bool persistentTypeOnly)
{
    GuardRecords records;

    CALL_LIBGUARD_INTERFACE(records = libguard::getAll(persistentTypeOnly);)

    return records;
}

const fs::path getGuardFilePath()
{
    fs::path guardfilePath;

    CALL_LIBGUARD_INTERFACE(guardfilePath = libguard::getGuardFilePath();)

    return guardfilePath;
}

} // namespace openpower_guard
} // namespace hw_isolation
