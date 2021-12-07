// SPDX-License-Identifier: Apache-2.0

#include "common/error_log.hpp"

#include <fcntl.h>
#include <fmt/format.h>

#include <phosphor-logging/elog.hpp>

#include <iomanip>
#include <sstream>

namespace hw_isolation
{
namespace error_log
{

using namespace phosphor::logging;

FFDCFile::FFDCFile(const FFDCFormat& format, const FFDCSubType& subType,
                   const FFDCVersion& version, const std::string& data) :
    _format(format),
    _subType(subType), _version(version),
    _fileName("/tmp/hwIsolationFFDCFile.XXXXXX"), _fd(-1), _data(data)
{
    prepareFFDCFile();
}

FFDCFile::~FFDCFile()
{
    removeFFDCFile();
}

int FFDCFile::getFD() const
{
    return _fd;
}

FFDCFormat FFDCFile::getFormat() const
{
    return _format;
}

FFDCSubType FFDCFile::getSubType() const
{
    return _subType;
}

FFDCVersion FFDCFile::getVersion() const
{
    return _version;
}

void FFDCFile::prepareFFDCFile()
{
    createFFDCFile();
    writeFFDCData();
    setFFDCFileSeekPos();
}

void FFDCFile::createFFDCFile()
{
    _fd = mkostemp(const_cast<char*>(_fileName.c_str()), O_RDWR);

    if (_fd == -1)
    {
        log<level::ERR>(fmt::format("Failed to create FFDC file [{}]"
                                    "errorno [{}] and errormsg [{}]",
                                    _fileName, errno, strerror(errno))
                            .c_str());
        throw std::runtime_error("Failed to create FFDC file");
    }
}

void FFDCFile::writeFFDCData()
{
    ssize_t rc = write(_fd, _data.c_str(), _data.size());

    if (rc == -1)
    {
        log<level::ERR>(fmt::format("Failed to write FFDC info in the "
                                    "file [{}], errorno[{}], errormsg[{}]",
                                    _fileName, errno, strerror(errno))
                            .c_str());
        throw std::runtime_error("Failed to write FFDC info");
    }
    else if (rc != static_cast<ssize_t>(_data.size()))
    {
        log<level::WARNING>(fmt::format("Could not write all FFDC info "
                                        "in the file[{}], written byte[{}] "
                                        "and total byt[{}]",
                                        _fileName, rc, _data.size())
                                .c_str());
    }
}

void FFDCFile::setFFDCFileSeekPos()
{
    int rc = lseek(_fd, 0, SEEK_SET);

    if (rc == -1)
    {
        log<level::ERR>(fmt::format("Failed to set SEEK_SET for the FFDC "
                                    "file[{}], errorno[{}] and errormsg({})",
                                    _fileName, errno, strerror(errno))
                            .c_str());
        throw std::runtime_error("Failed to set SEEK_SET for FFDC file");
    }
}

void FFDCFile::removeFFDCFile()
{
    close(_fd);
    std::remove(_fileName.c_str());
}

FFDCFiles::FFDCFiles(const bool collectTraces, const json& calloutsDetails)
{
    // Create FFDCFile for the traces if requested to collect
    if (collectTraces)
    {
        try
        {
            createFFDCFileForTraces();
        }
        catch (const std::exception& e)
        {
            /**
             * Don't throw the exception, we should create FFDCFiles as much as
             * possible to create the error log.
             */
            log<level::ERR>(fmt::format("Exception [{}]", e.what()).c_str());
        }
    }

    // Create FFDCFile for the JSON data if it is filled
    if (!calloutsDetails.is_null())
    {
        try
        {
            createFFDCFileforCallouts(calloutsDetails);
        }
        catch (const std::exception& e)
        {
            /**
             * Don't throw the exception, we should create FFDCFiles as much as
             * possible to create the error log.
             */
            log<level::ERR>(fmt::format("Exception [{}]", e.what()).c_str());
        }
    }
}

std::optional<std::string>
    FFDCFiles::sdjGetTraceFieldValue(sd_journal* journal,
                                     const std::string& fieldName)
{
    const char* data{nullptr};
    size_t length{0};

    if (auto rc =
            sd_journal_get_data(journal, fieldName.c_str(),
                                reinterpret_cast<const void**>(&data), &length);
        rc == 0)
    {
        // Get field value
        std::string fieldValue(data, length);

        // The data returned  by sd_journal_get_data will be prefixed with the
        // field name and "=".
        if (auto fieldValPos = fieldValue.find('=');
            fieldValPos != std::string::npos)
        {
            // Just return field value alone instead of with field name
            return fieldValue.substr(fieldValPos + 1);
        }
        else
        {
            // Should not happen as per sd_journal_get_data documentation
            log<level::ERR>(
                fmt::format("Failed to find the journal field "
                            "separator [=] in the retrieved field [{}]",
                            fieldValue)
                    .c_str());
        }
    }
    else
    {
        log<level::ERR>(
            fmt::format("Failed to get the given journal "
                        "field [{}] value errorno [{}] and errormsg [{}]",
                        fieldName, rc, strerror(rc))
                .c_str());
    }
    return std::nullopt;
}

std::optional<std::vector<std::string>>
    FFDCFiles::sdjGetTraces(const std::string& fieldName,
                            const std::string& fieldValue,
                            const unsigned int maxReqTraces)
{
    // First get the systemd journal context
    sd_journal* journal;
    if (auto rc = sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY); rc == 0)
    {
        std::vector<std::string> traces;

        // Look the systemd journal entries from the reverse order
        // so that we will get requested latest number of traces
        SD_JOURNAL_FOREACH_BACKWARDS(journal)
        {
            auto retValue = sdjGetTraceFieldValue(journal, fieldName);

            if (!retValue.has_value() || *retValue != fieldValue)
            {
                // The retrieved journal entry is not expected one
                continue;
            }

            // Get SYSLOG_IDENTIFIER field (process that logged trace)
            std::string sysLogId;
            if (fieldName == std::string("SYSLOG_IDENTIFIER"))
            {
                sysLogId = fieldValue;
            }
            else
            {
                if (retValue =
                        sdjGetTraceFieldValue(journal, "SYSLOG_IDENTIFIER");
                    retValue.has_value())
                {
                    sysLogId = *retValue;
                }
            }

            // Get _PID field
            std::string pid;
            if (retValue = sdjGetTraceFieldValue(journal, "_PID");
                retValue.has_value())
            {
                pid = *retValue;
            }

            // Get MESSAGE field
            std::string message;
            if (retValue = sdjGetTraceFieldValue(journal, "MESSAGE");
                retValue.has_value())
            {
                message = *retValue;
            }

            // Get timestamp
            std::string timeStamp;
            uint64_t usec{0};
            if (0 == sd_journal_get_realtime_usec(journal, &usec))
            {
                // Convert realtime microseconds to date format
                // E.g.: Dec 07 2021 15:48:29
                std::time_t timeInSecs = usec / 1000000;
                std::stringstream ss;
                ss << std::put_time(std::localtime(&timeInSecs),
                                    "%b %d %Y %H:%M:%S");

                timeStamp = ss.str();
            }

            // Format: Timestamp : ProcessName[ProcessPID] : Message
            std::stringstream trace;
            trace << timeStamp << " : " << sysLogId << "[" << pid
                  << "] : " << message;

            /**
             * Add the retrieved trace before first trace since we usually
             * look at the traces from bottom to up to get to know
             * whats happend from the current trace to previous traces.
             *
             * The systemd journal entries are fetched in the reverse order.
             */
            traces.emplace(traces.begin(), trace.str());

            // Don't go for the next systemd journal entry if the maximum
            // required traces are fetched
            if (traces.size() == maxReqTraces)
            {
                break;
            }
        }

        // Close journal when fetched the systemd journal traces
        sd_journal_close(journal);

        if (!traces.empty())
        {
            return traces;
        }
        else
        {
            log<level::INFO>(
                fmt::format("Dont have any systemd journal traces"
                            "for the given field name [{}] and value [{}].",
                            fieldName, fieldValue)
                    .c_str());
        }
    }
    else
    {
        log<level::ERR>(
            fmt::format("Failed to get the systemd journal "
                        "traces for the given field name [{}] and value [{}]. "
                        "ErrorNo [{}] and ErrorMsg [{}]",
                        fieldName, fieldValue, rc, strerror(rc))
                .c_str());
    }

    return std::nullopt;
}

void FFDCFiles::createFFDCFileForTraces()
{
    // Add required applications to get their traces
    static const std::vector<std::string> apps{"openpower-hw-isolation"};

    for (const auto& app : apps)
    {
        // By default we can get 10 traces
        auto traces = sdjGetTraces("SYSLOG_IDENTIFIER", app);

        if (traces.has_value() && !traces->empty())
        {
            std::string data;
            for (const auto& trace : *traces)
            {
                data.append(trace);
                if (!trace.ends_with('\n'))
                {
                    data.append("\n");
                }
            }

            // FFDC Subtype and Version is "0" for the FFDCFormat::Text
            _ffdcFiles.emplace_back(
                std::make_unique<FFDCFile>(FFDCFormat::Text, 0, 0, data));
        }
    }
}

void FFDCFiles::createFFDCFileforCallouts(const json& calloutsDetails)
{
    // FFDC Subtype and Version should be "0xCA" and "0x01" respectively
    // for the callouts
    _ffdcFiles.emplace_back(std::make_unique<FFDCFile>(
        FFDCFormat::JSON, 0xCA, 0x01, calloutsDetails.dump()));
}

void FFDCFiles::transformFFDCFiles(FFDCFilesInfo& ffdcFilesInfo)
{
    std::transform(_ffdcFiles.begin(), _ffdcFiles.end(),
                   std::back_inserter(ffdcFilesInfo), [](const auto& ffdcFile) {
                       return std::make_tuple(ffdcFile->getFormat(),
                                              ffdcFile->getSubType(),
                                              ffdcFile->getVersion(),
                                              ffdcFile->getFD());
                   });
}

} // namespace error_log
} // namespace hw_isolation
