#pragma once

#include <nlohmann/json.hpp>

namespace openpower::faultlog
{
/**
 * @class DeconfigRecords
 *
 * Capture Deconfig records in JSON file
 *
 * Filed code override is a method of enabling only a
 * limited number of processor cores in the system.
 */
class DeconfigRecords
{
  private:
    DeconfigRecords() = delete;
    DeconfigRecords(const DeconfigRecords&) = delete;
    DeconfigRecords& operator=(const DeconfigRecords&) = delete;
    DeconfigRecords(DeconfigRecords&&) = delete;
    DeconfigRecords& operator=(DeconfigRecords&&) = delete;
    ~DeconfigRecords() = delete;

  public:
    /** @brief Poupulate target details that have deconfiguredByEid set
     *
     *  @param[in] jsonNag - Update JSON deconfigure records
     */
    static void populate(nlohmann::json& jsonNag);
};
} // namespace openpower::faultlog
