#include "TestSupport.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <array>
#include <memory>
#include <string_view>
#include <vector>

namespace skygate::ephemeris::tests {
namespace {

const CelestialBody* findBodyById(
    const std::vector<CelestialBody>& bodies,
    const std::string_view id
)
{
    for (const auto& body : bodies) {
        if (body.id == id) {
            return &body;
        }
    }

    return nullptr;
}

bool runCatalogRowParserTests()
{
    bool success = true;

    std::unique_ptr<IStarCatalog> bundledCatalog = createBundledStarCatalog();
    success = expectTrue(bundledCatalog != nullptr, "Bundled catalog should be created") && success;
    if (bundledCatalog == nullptr) {
        return false;
    }

    const auto bundledBodies = bundledCatalog->bodies();
    success = expectTrue(!bundledBodies.empty(), "Bundled catalog should include at least one body") && success;
    success = expectTrue(
        bundledBodies.size() == 25,
        "Bundled catalog should include expected initial body set"
    ) && success;

    const auto* sun = findBodyById(bundledBodies, "sun");
    const auto* sirius = findBodyById(bundledBodies, "sirius");
    const auto* orion = findBodyById(bundledBodies, "orion");
    success = expectTrue(
        sun != nullptr && sun->type == CelestialBodyType::Sun,
        "Bundled catalog should contain Sun"
    ) && success;
    success = expectTrue(
        sirius != nullptr && sirius->type == CelestialBodyType::Star,
        "Bundled catalog should contain Sirius"
    ) && success;
    success = expectTrue(
        orion != nullptr && orion->type == CelestialBodyType::Constellation,
        "Bundled catalog should contain Orion constellation"
    ) && success;

    std::unique_ptr<IStarCatalog> mixedRowsCatalog = createStarCatalogFromRows(
        "# comment row\n"
        "short|TooShort|Star\n"
        "|NoId|Star|1.0\n"
        "nodisplay||Star|1.0\n"
        "badtype|BadType|Asteroid|1.0\n"
        "badmag|BadMag|Star|nan\n"
        "vega| Vega |Star|0.03|18.6156|38.7837\n"
        "partial|Partial|Star|2.0|1.2\n"
        "spaced_id | Spaced Name | Moon | -12.0\n"
    );
    success = expectTrue(mixedRowsCatalog != nullptr, "Mixed rows catalog should parse valid rows") && success;
    if (mixedRowsCatalog != nullptr) {
        const auto parsedBodies = mixedRowsCatalog->bodies();
        success = expectTrue(parsedBodies.size() == 3, "Mixed rows catalog should keep only valid rows") && success;

        const auto* parsedVega = findBodyById(parsedBodies, "vega");
        success = expectTrue(parsedVega != nullptr, "Mixed rows catalog should contain Vega") && success;
        if (parsedVega != nullptr) {
            success = expectTrue(parsedVega->displayName == "Vega", "Display names should be trimmed") && success;
            success = expectTrue(
                parsedVega->fixedEquatorial.has_value(),
                "Rows with RA and Dec should set fixed equatorial coordinates"
            ) && success;
        }

        const auto* partial = findBodyById(parsedBodies, "partial");
        success = expectTrue(partial != nullptr, "Mixed rows catalog should contain partial row") && success;
        if (partial != nullptr) {
            success = expectTrue(
                !partial->fixedEquatorial.has_value(),
                "Rows missing Dec should not set fixed equatorial coordinates"
            ) && success;
        }

        const auto* spacedId = findBodyById(parsedBodies, "spaced_id");
        success = expectTrue(spacedId != nullptr, "ID values should be trimmed") && success;
        if (spacedId != nullptr) {
            success = expectTrue(
                spacedId->displayName == "Spaced Name",
                "Whitespace around display names should be trimmed"
            ) && success;
            success = expectTrue(
                spacedId->type == CelestialBodyType::Moon,
                "Moon type should parse correctly"
            ) && success;
        }
    }

    std::unique_ptr<IStarCatalog> downloadedLikeCatalog = createStarCatalogFromRows(
        "vega|Vega|Star|0.03|18.6156|38.7837\n"
        "orion|Orion|Constellation|1.6\n"
    );
    success = expectTrue(downloadedLikeCatalog != nullptr, "Downloaded-like catalog text should parse") && success;
    if (downloadedLikeCatalog != nullptr) {
        const auto parsedBodies = downloadedLikeCatalog->bodies();
        success = expectTrue(parsedBodies.size() == 2, "Downloaded-like catalog should contain two rows") && success;
        success = expectTrue(
            parsedBodies[0].fixedEquatorial.has_value(),
            "Downloaded-like catalog should parse optional RA/Dec coordinates"
        ) && success;
    }

    std::unique_ptr<IStarCatalog> emptyRowsCatalog = createStarCatalogFromRows("");
    success = expectTrue(emptyRowsCatalog == nullptr, "Empty row input should fail to create catalog") && success;

    std::unique_ptr<IStarCatalog> invalidRowsCatalog = createStarCatalogFromRows(
        "# only comments\n"
        "broken_row_without_pipes\n"
        "still|bad|row\n"
    );
    success = expectTrue(invalidRowsCatalog == nullptr, "Rows with no valid bodies should return null") && success;

    return success;
}

bool runHygCsvParserTests()
{
    bool success = true;

    std::unique_ptr<IStarCatalog> hygCatalog = createStarCatalogFromHygCsv(
        "id,hip,proper,ra,dec,mag\n"
        "1,32349,Sirius,6.7525,-16.7161,-1.46\n"
        "2,24608,Capella,5.2782,45.9979,0.08\n"
    );
    success = expectTrue(hygCatalog != nullptr, "HYG CSV catalog should parse") && success;
    if (hygCatalog != nullptr) {
        const auto parsedBodies = hygCatalog->bodies();
        success = expectTrue(parsedBodies.size() == 2, "HYG CSV parser should load expected rows") && success;
        success = expectTrue(
            parsedBodies[0].fixedEquatorial.has_value(),
            "HYG CSV parser should attach fixed equatorial coordinates"
        ) && success;
        success = expectTrue(
            parsedBodies[0].id == "hip_32349",
            "HYG CSV parser should prefer HIP-based IDs when available"
        ) && success;
    }

    std::unique_ptr<IStarCatalog> hygWithoutHipCatalog = createStarCatalogFromHygCsv(
        "id,proper,ra,dec,mag\n"
        "77,NoHipStar,1.5,2.5,4.2\n"
    );
    success = expectTrue(hygWithoutHipCatalog != nullptr, "HYG CSV parser should support rows without HIP") && success;
    if (hygWithoutHipCatalog != nullptr) {
        const auto parsedBodies = hygWithoutHipCatalog->bodies();
        success = expectTrue(parsedBodies.size() == 1, "HYG CSV parser should load row without HIP") && success;
        if (!parsedBodies.empty()) {
            success = expectTrue(
                parsedBodies[0].id == "hyg_77",
                "HYG CSV parser should fall back to HYG ID when HIP is missing"
            ) && success;
        }
    }

    std::unique_ptr<IStarCatalog> hygFallbackCatalog = createStarCatalogFromHygCsv(
        "hip,id,proper,bf,ra,dec,mag\n"
        ",,Unnamed,,1.0,2.0,3.0\n"
        ",99,,Beta,4.0,5.0,6.0\n"
        "77,,,,7.0,8.0,9.0\n"
        "2,101,\"Alpha, Beta\",,10.0,11.0,1.5\n"
        "3,102,\"Quote \"\"Star\"\"\",,12.0,13.0,2.5\n"
        "1,100,Skipped,,1.0,2.0,\n"
    );
    success = expectTrue(hygFallbackCatalog != nullptr, "HYG CSV should parse fallback and quoted fields") && success;
    if (hygFallbackCatalog != nullptr) {
        const auto parsedBodies = hygFallbackCatalog->bodies();
        success = expectTrue(parsedBodies.size() == 5, "HYG fallback catalog should keep five valid rows") && success;

        const auto* autoBody = findBodyById(parsedBodies, "hyg_auto_1");
        success = expectTrue(autoBody != nullptr, "Missing HIP and ID should generate automatic ID") && success;
        if (autoBody != nullptr) {
            success = expectTrue(autoBody->displayName == "Unnamed", "Proper name should be preferred as display name") && success;
        }

        const auto* hygIdBody = findBodyById(parsedBodies, "hyg_99");
        success = expectTrue(hygIdBody != nullptr, "Missing HIP should fall back to HYG ID") && success;
        if (hygIdBody != nullptr) {
            success = expectTrue(hygIdBody->displayName == "Beta", "BF value should be fallback display name") && success;
        }

        const auto* hipBody = findBodyById(parsedBodies, "hip_77");
        success = expectTrue(hipBody != nullptr, "HIP value should be preferred for IDs") && success;
        if (hipBody != nullptr) {
            success = expectTrue(hipBody->displayName == "HIP 77", "HIP value should be fallback display name") && success;
        }

        const auto* quotedNameBody = findBodyById(parsedBodies, "hip_2");
        success = expectTrue(quotedNameBody != nullptr, "Quoted row should parse") && success;
        if (quotedNameBody != nullptr) {
            success = expectTrue(quotedNameBody->displayName == "Alpha, Beta", "Quoted commas should parse inside fields") && success;
        }

        const auto* escapedQuoteBody = findBodyById(parsedBodies, "hip_3");
        success = expectTrue(escapedQuoteBody != nullptr, "Escaped quote row should parse") && success;
        if (escapedQuoteBody != nullptr) {
            success = expectTrue(escapedQuoteBody->displayName == "Quote \"Star\"", "Escaped quotes should be unescaped") && success;
            success = expectTrue(
                escapedQuoteBody->fixedEquatorial.has_value(),
                "Parsed HYG row should include fixed equatorial coordinates"
            ) && success;
        }
    }

    std::unique_ptr<IStarCatalog> headerOnlyCatalog =
        createStarCatalogFromHygCsv("hip,id,proper,ra,dec,mag\n");
    success = expectTrue(headerOnlyCatalog == nullptr, "HYG header without data should not create catalog") && success;

    std::unique_ptr<IStarCatalog> malformedHygCatalog = createStarCatalogFromHygCsv(
        "id,name\n"
        "1,NoCoordinates\n"
    );
    success = expectTrue(malformedHygCatalog == nullptr, "HYG rows without coordinate columns should be rejected") && success;

    return success;
}

bool runHygCsvGzipTests()
{
    bool success = true;

    constexpr std::array<unsigned char, 73> kCompressedCsv {
        0x1f, 0x8b, 0x08, 0x00, 0x77, 0xa9, 0x86, 0x69, 0x00, 0x03, 0xcb, 0x4c,
        0xd1, 0xc9, 0xc8, 0x2c, 0xd0, 0x29, 0x28, 0xca, 0x2f, 0x48, 0x2d, 0xd2,
        0x29, 0x4a, 0xd4, 0x49, 0x49, 0x4d, 0xd6, 0xc9, 0x4d, 0x4c, 0xe7, 0x32,
        0xd7, 0x31, 0x31, 0xd2, 0x09, 0x49, 0x2d, 0x2e, 0x09, 0x2e, 0x49, 0x2c,
        0xd2, 0x31, 0xd4, 0x33, 0x32, 0xd5, 0xd1, 0x35, 0xd2, 0x33, 0xd5, 0x31,
        0xd6, 0x33, 0xe2, 0x02, 0x00, 0xb9, 0xd5, 0xee, 0x71, 0x35, 0x00, 0x00,
        0x00
    };

    const std::string_view compressedData(
        reinterpret_cast<const char*>(kCompressedCsv.data()),
        kCompressedCsv.size()
    );

    std::unique_ptr<IStarCatalog> gzipCatalog = createStarCatalogFromHygCsvGzip(compressedData);
    success = expectTrue(gzipCatalog != nullptr, "Valid gzip HYG data should parse") && success;
    if (gzipCatalog != nullptr) {
        const auto parsedBodies = gzipCatalog->bodies();
        success = expectTrue(parsedBodies.size() == 1, "Gzip HYG should parse one star row") && success;
        if (!parsedBodies.empty()) {
            success = expectTrue(parsedBodies[0].id == "hip_42", "Gzip HYG parser should apply HIP-based ID") && success;
            success = expectTrue(
                parsedBodies[0].fixedEquatorial.has_value(),
                "Gzip HYG row should include fixed equatorial coordinates"
            ) && success;
            if (parsedBodies[0].fixedEquatorial.has_value()) {
                success = expectNear(
                    parsedBodies[0].fixedEquatorial->rightAscensionHours,
                    1.25,
                    1e-8,
                    "Gzip HYG parser should parse RA values"
                ) && success;
                success = expectNear(
                    parsedBodies[0].fixedEquatorial->declinationDeg,
                    -2.5,
                    1e-8,
                    "Gzip HYG parser should parse Dec values"
                ) && success;
            }
        }
    }

    std::unique_ptr<IStarCatalog> emptyGzipCatalog = createStarCatalogFromHygCsvGzip("");
    success = expectTrue(emptyGzipCatalog == nullptr, "Empty gzip input should fail") && success;

    std::unique_ptr<IStarCatalog> malformedGzipCatalog = createStarCatalogFromHygCsvGzip("not-a-gzip-stream");
    success = expectTrue(malformedGzipCatalog == nullptr, "Malformed gzip input should fail") && success;

    return success;
}

}  // namespace

bool runStarCatalogTests()
{
    bool success = true;
    success = runCatalogRowParserTests() && success;
    success = runHygCsvParserTests() && success;
    success = runHygCsvGzipTests() && success;
    return success;
}

}  // namespace skygate::ephemeris::tests
