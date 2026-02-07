#include "TestSupport.hpp"

#include <iostream>

namespace skygate::ephemeris::tests {

bool runStarCatalogTests();
bool runEphemerisEngineTests();

}  // namespace skygate::ephemeris::tests

int main()
{
    using namespace skygate::ephemeris::tests;

    const bool success = runAll({
        {"StarCatalog", runStarCatalogTests},
        {"EphemerisEngine", runEphemerisEngineTests},
    });

    if (!success) {
        return 1;
    }

    std::cout << "All skygate-ephemeris tests passed\n";
    return 0;
}
