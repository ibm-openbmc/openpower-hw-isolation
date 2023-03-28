#include <faultlog_policy.hpp>
#include <faultlog_records.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/bus.hpp>

#include <iostream>
#include <vector>

using namespace openpower::faultlog;
using ::nlohmann::json;

int main(int /*arg*/, char** /*argv*/)
{
    try
    {
        auto bus = sdbusplus::bus::new_default();

        nlohmann::json faultLogJson = json::array();
        std::vector<int> processedEid;

        // add hardware isolation records to json file
        FaultLogRecords::populate(bus, faultLogJson, processedEid);
        FaultLogPolicy::populate(bus, faultLogJson);
        std::cout << "Nag data is " << std::endl;
        std::cout << faultLogJson.dump(2) << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
    return 0;
}
