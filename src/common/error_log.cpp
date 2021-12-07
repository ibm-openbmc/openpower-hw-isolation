// SPDX-License-Identifier: Apache-2.0

#include "common/error_log.hpp"

#include <fcntl.h>
#include <fmt/format.h>

#include <phosphor-logging/elog.hpp>

namespace hw_isolation
{
namespace error_log
{

using namespace phosphor::logging;

FFDCFile::FFDCFile(const FFDCFormat& format, const FFDCSubType& subType,
                   const FFDCVersion& version, const std::string& data) :
    _format(format),
    _subType(subType), _version(version), _fd(-1), _data(data)
{
    // Prepare temporary file name based on the given FFDC format
    _fileName = (std::string("/tmp/hwIsolation") +
                 CreateIface::convertFFDCFormatToString(_format) +
                 std::string(".XXXXXX"));

    prepareFFDCFile();
}

FFDCFile::~FFDCFile()
{
    removeFFDCFile();
}

int FFDCFile::getFFDCFileFD() const
{
    return _fd;
}

FFDCFormat FFDCFile::getFFDCFormat() const
{
    return _format;
}

FFDCSubType FFDCFile::getFFDCSubtype() const
{
    return _subType;
}

FFDCVersion FFDCFile::getFFDCVersion() const
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

} // namespace error_log
} // namespace hw_isolation
