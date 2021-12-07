// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <xyz/openbmc_project/Logging/Create/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

namespace hw_isolation
{
namespace error_log
{

using CreateIface = sdbusplus::xyz::openbmc_project::Logging::server::Create;

using FFDCFormat = CreateIface::FFDCFormat;
using FFDCSubType = uint8_t;
using FFDCVersion = uint8_t;
using FFDCFileFD = sdbusplus::message::unix_fd;

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

} // namespace error_log
} // namespace hw_isolation
