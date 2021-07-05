// SPDX-License-Identifier: Apache-2.0

extern "C"
{
#include <libpdbg.h>
}

#include "config.h"

#include "phal_devtree_utils.hpp"

#include <fmt/format.h>
#include <stdlib.h>

#include <phosphor-logging/elog-errors.hpp>

#include <stdexcept>

namespace hw_isolation
{
namespace devtree
{

using namespace phosphor::logging;

void initPHAL()
{
    // Set PDBG_DTB environment variable to use interested phal cec device tree
    if (setenv("PDBG_DTB", PHAL_DEVTREE, 1))
    {
        log<level::ERR>(
            fmt::format(
                "Failed to set PDBG_DTB with ErrNo [{}] and ErrMsg [{}]", errno,
                strerror(errno))
                .c_str());
        throw std::runtime_error(
            "Failed to set PDBG_DTB while trying to init PHAL");
    }

    // Set log level to info
    pdbg_set_loglevel(PDBG_ERROR);

    /**
     * Passing fdt argument as NULL so, pdbg will use PDBG_DTB environment
     * variable to get interested phal cec device tree instead of default pdbg
     * device tree.
     */
    if (!pdbg_targets_init(NULL))
    {
        throw std::runtime_error("pdbg target initialization failed");
    }
}

} // namespace devtree
} // namespace hw_isolation
