#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <cerrno>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {
namespace {

class BundledStarCatalog final : public IStarCatalog {
public:
    BundledStarCatalog()
        : m_bodies(loadBundledBodies())
    {
    }

    [[nodiscard]] std::vector<CelestialBody> bodies() const override
    {
        return m_bodies;
    }

private:
    static std::vector<CelestialBody> loadBundledBodies()
    {
        constexpr std::string_view kCatalogRows =
            "sun|Sun|Sun|-26.74\n"
            "moon|Moon|Moon|-12.74\n"
            "mercury|Mercury|Planet|-1.90\n"
            "venus|Venus|Planet|-4.92\n"
            "mars|Mars|Planet|-2.94\n"
            "jupiter|Jupiter|Planet|-2.94\n"
            "saturn|Saturn|Planet|-0.55\n"
            "sirius|Sirius|Star|-1.46\n"
            "canopus|Canopus|Star|-0.74\n"
            "arcturus|Arcturus|Star|-0.05\n"
            "vega|Vega|Star|0.03\n"
            "capella|Capella|Star|0.08\n"
            "rigel|Rigel|Star|0.12\n"
            "procyon|Procyon|Star|0.34\n"
            "betelgeuse|Betelgeuse|Star|0.50\n";

        std::vector<CelestialBody> bodies;
        std::size_t cursor = 0;

        while (cursor < kCatalogRows.size()) {
            const std::size_t newline = kCatalogRows.find('\n', cursor);
            const std::size_t lineEnd = newline == std::string_view::npos ? kCatalogRows.size() : newline;
            const std::string_view line = kCatalogRows.substr(cursor, lineEnd - cursor);
            if (const auto body = parseBodyRow(line); body.has_value()) {
                bodies.push_back(*body);
            }

            if (newline == std::string_view::npos) {
                break;
            }

            cursor = newline + 1;
        }

        return bodies;
    }

    static std::optional<CelestialBody> parseBodyRow(const std::string_view row)
    {
        const std::size_t firstPipe = row.find('|');
        if (firstPipe == std::string_view::npos) {
            return std::nullopt;
        }

        const std::size_t secondPipe = row.find('|', firstPipe + 1);
        if (secondPipe == std::string_view::npos) {
            return std::nullopt;
        }

        const std::size_t thirdPipe = row.find('|', secondPipe + 1);
        if (thirdPipe == std::string_view::npos) {
            return std::nullopt;
        }

        const std::string_view id = row.substr(0, firstPipe);
        const std::string_view displayName = row.substr(firstPipe + 1, secondPipe - firstPipe - 1);
        const std::string_view typeText = row.substr(secondPipe + 1, thirdPipe - secondPipe - 1);
        const std::string_view magnitudeText = row.substr(thirdPipe + 1);

        const auto type = parseType(typeText);
        if (!type.has_value()) {
            return std::nullopt;
        }

        std::string magnitudeBuffer(magnitudeText);
        char* magnitudeParseEnd = nullptr;
        errno = 0;
        const double visualMagnitude = std::strtod(magnitudeBuffer.c_str(), &magnitudeParseEnd);
        if (
            errno != 0 || magnitudeParseEnd == nullptr ||
            magnitudeParseEnd != magnitudeBuffer.c_str() + magnitudeBuffer.size()
        ) {
            return std::nullopt;
        }

        CelestialBody body;
        body.id = std::string(id);
        body.displayName = std::string(displayName);
        body.type = *type;
        body.visualMagnitude = visualMagnitude;
        return body;
    }

    static std::optional<CelestialBodyType> parseType(const std::string_view typeText)
    {
        if (typeText == "Star") {
            return CelestialBodyType::Star;
        }

        if (typeText == "Planet") {
            return CelestialBodyType::Planet;
        }

        if (typeText == "Moon") {
            return CelestialBodyType::Moon;
        }

        if (typeText == "Sun") {
            return CelestialBodyType::Sun;
        }

        return std::nullopt;
    }

private:
    std::vector<CelestialBody> m_bodies;
};

}  // namespace

std::unique_ptr<IStarCatalog> createBundledStarCatalog()
{
    return std::make_unique<BundledStarCatalog>();
}

}  // namespace skygate::ephemeris
