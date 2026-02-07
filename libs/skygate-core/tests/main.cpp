#include "TestSupport.hpp"

#include <iostream>

namespace skygate::core::tests {

bool runGeoLocationTests();
bool runProjectionTests();

}  // namespace skygate::core::tests

int main()
{
    using namespace skygate::core::tests;

    const bool success = runAll({
        {"GeoLocation", runGeoLocationTests},
        {"Projection", runProjectionTests},
    });

    if (!success) {
        return 1;
    }

    std::cout << "All skygate-core tests passed\n";
    return 0;
}
