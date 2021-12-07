// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <systemd/sd-journal.h>

#include <nlohmann/json.hpp>
#include <xyz/openbmc_project/Logging/Create/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

namespace hw_isolation
{
namespace error_log
{

using json = nlohmann::json;

using Level = sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;
using CreateIface = sdbusplus::xyz::openbmc_project::Logging::server::Create;

using FFDCFormat = CreateIface::FFDCFormat;
using FFDCSubType = uint8_t;
using FFDCVersion = uint8_t;
using FFDCFileFD = sdbusplus::message::unix_fd;

using FFDCFileInfo =
    std::tuple<FFDCFormat, FFDCSubType, FFDCVersion, FFDCFileFD>;
using FFDCFilesInfo = std::vector<FFDCFileInfo>;

constexpr auto HwIsolationGenericErrMsg =
    "org.open_power.HardwareIsolation.Error";
constexpr auto CollectTraces = true;

/**
 * @class FFDCFile
 *
 * @brief This class is used to create ffdc file with data
 */
class FFDCFile
{
  public:
    FFDCFile() = delete;
    FFDCFile(const FFDCFile&) = delete;
    FFDCFile& operator=(const FFDCFile&) = delete;
    FFDCFile(FFDCFile&&) = delete;
    FFDCFile& operator=(FFDCFile&&) = delete;

    /**
     * @brief Used to create the FFDC file with the given format and data
     *
     * @param[in] format - The FFDC file format.
     * @param[in] subType - The FFDC file subtype.
     * @param[in] version - The FFDC file version.
     * @param[in] data - The FFDC data to write in the FFDC file.
     */
    explicit FFDCFile(const FFDCFormat& format, const FFDCSubType& subType,
                      const FFDCVersion& version, const std::string& data);

    /**
     * @brief Used to remove created ffdc file.
     */
    ~FFDCFile();

    /**
     * @brief Used to get created ffdc file descriptor id.
     *
     * @return file descriptor id
     */
    int getFD() const;

    /**
     * @brief Used to get created ffdc file format.
     *
     * @return ffdc file format.
     */
    FFDCFormat getFormat() const;

    /**
     * @brief Used to get created ffdc file subtype.
     *
     * @return ffdc file subtype.
     */
    FFDCSubType getSubType() const;

    /**
     * @brief Used to get created ffdc file version.
     *
     * @return ffdc file version.
     */
    FFDCVersion getVersion() const;

  private:
    /**
     * @brief Used to store ffdc format.
     */
    FFDCFormat _format;

    /**
     * @brief Used to store ffdc subtype.
     */
    FFDCSubType _subType;

    /**
     * @brief Used to store ffdc version.
     */
    FFDCVersion _version;

    /**
     * @brief Used to store unique ffdc file name.
     */
    std::string _fileName;

    /**
     * @brief Used to store created ffdc file descriptor id.
     */
    FFDCFileFD _fd;

    /**
     * @brief Used to store ffdc data and write into the file
     */
    std::string _data;

    /**
     * @brief Used to create ffdc file to pass in the error log request
     *
     * @return Throw an exception on failure
     */
    void prepareFFDCFile();

    /**
     * @brief Create unique ffdc file.
     *
     * @return Throw an exception on failure
     */
    void createFFDCFile();

    /**
     * @brief Used write ffdc data into the created file.
     *
     * @return Throw an exception on failure
     */
    void writeFFDCData();

    /**
     * @brief Used set ffdc file seek position begining to consume by the
     *        error log
     *
     * @return Throw an exception on failure
     */
    void setFFDCFileSeekPos();

    /**
     * @brief Used to remove created ffdc file.
     *
     * @return Throw an exception on failure
     */
    void removeFFDCFile();

}; // end of FFDCFile class

/**
 * @class FFDCFiles
 *
 * @brief This class is used to manage the created ffdc files
 */
class FFDCFiles
{
  public:
    FFDCFiles() = delete;
    FFDCFiles(const FFDCFiles&) = delete;
    FFDCFiles& operator=(const FFDCFiles&) = delete;
    FFDCFiles(FFDCFiles&&) = delete;
    FFDCFiles& operator=(FFDCFiles&&) = delete;

    /**
     * @brief Used to create the FFDC files based on the given inputs
     *
     * @param[in] collectTraces - Used to create the FFDC file for
     *                            the traces if required.
     * @param[in] calloutsDetails - Used to create the FFDC file for
     *                              the callouts if exist.
     */
    explicit FFDCFiles(const bool collectTraces, const json& calloutsDetails);

    /**
     * @brief Used to get the created FFDC files details to add in the error
     *        log
     *
     * @param[out] ffdcFilesInfo - Used to return FFDCFiles with its
     *                             information
     *
     * @return void
     */
    void transformFFDCFiles(FFDCFilesInfo& ffdcFilesInfo);

  private:
    /**
     * @brief Used to store the list of created FFDCFile
     */
    std::vector<std::unique_ptr<FFDCFile>> _ffdcFiles;

    /**
     * @brief Helper API to get the given field value from
     *        the systemd journal context
     *
     * @param[in] journal - The systemd journal to get the field value
     * @param[in] fieldName - The field name to get its value
     *
     * @return The field value on success
     *         Empty optional on failures
     */
    std::optional<std::string>
        sdjGetTraceFieldValue(sd_journal* journal,
                              const std::string& fieldName);

    /**
     * @brief Helper API to get the last requested number of traces
     *        from systemd journal entries based on the given field name
     *        and value. By default, it will collect last 10 traces.
     *
     * @param[in] fieldName - The field name to search.
     * @param[in] fieldValue - The field value to search.
     * @param[in] maxReqTraces - The maximum number of traces to fetch.
     *
     * @return The list of traces from the systemd journal on success
     *         Emptry optional on failure
     */
    std::optional<std::vector<std::string>>
        sdjGetTraces(const std::string& fieldName,
                     const std::string& fieldValue,
                     const unsigned int maxReqTraces = 10);

    /**
     * @brief Used to collect the systemd journal traces and create the
     *        FFDCFile
     *
     * @return Throw Exceptions on failure.
     */
    void createFFDCFileForTraces();

    /**
     * @brief Used to create the FFDC file with the given callout details
     *
     * @param[in] calloutsDetails - callouts detailsto create ffdc file
     *
     * @return Throw Exceptions on failure.
     */
    void createFFDCFileforCallouts(const json& calloutsDetails);

}; // end of FFDCFiles class

/**
 * @brief Create the error log with additional FFDC data and callout details
 *
 * @param[in] errMsg - the error message id which is be defined
 *                     in the error log message registries.
 * @param[in] errSeverity - The error log severity
 * @param[in] collectTraces - collect traces to add in the error log
 * @param[in] calloutsDetails - callouts details to add in the error log
 *
 * @return void
 *
 * @note If the caller doesn't pass the collectTraces or calloutsDetails
 * values then, this API won't add those sections in the error log.
 */
void createErrorLog(const std::string& errMsg, const Level& errSeverity,
                    const bool collectTraces = CollectTraces,
                    const json& calloutsDetails = {});

} // namespace error_log
} // namespace hw_isolation
