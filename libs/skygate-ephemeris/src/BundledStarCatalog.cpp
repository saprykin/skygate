#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <cerrno>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <zlib.h>

namespace skygate::ephemeris {
namespace {

class InMemoryStarCatalog final : public IStarCatalog {
public:
    explicit InMemoryStarCatalog(std::vector<CelestialBody> bodies)
        : m_bodies(std::move(bodies))
    {
    }

    [[nodiscard]] std::vector<CelestialBody> bodies() const override
    {
        return m_bodies;
    }

private:
    std::vector<CelestialBody> m_bodies;
};

constexpr std::size_t kMaxHygBodyCount = 25000;

std::string_view trimAsciiWhitespace(std::string_view value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
        value.remove_suffix(1);
    }
    return value;
}

std::string toLowerAscii(std::string_view value)
{
    std::string lowered;
    lowered.reserve(value.size());
    for (const char c : value) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return lowered;
}

std::optional<double> parseFiniteDouble(std::string_view text)
{
    const std::string_view normalized = trimAsciiWhitespace(text);
    if (normalized.empty()) {
        return std::nullopt;
    }

    std::string buffer(normalized);
    char* parseEnd = nullptr;
    errno = 0;
    const double value = std::strtod(buffer.c_str(), &parseEnd);
    if (
        errno != 0 || parseEnd == nullptr ||
        parseEnd != buffer.c_str() + buffer.size() ||
        !std::isfinite(value)
    ) {
        return std::nullopt;
    }

    return value;
}

std::vector<std::string_view> splitPipeColumns(const std::string_view row)
{
    std::vector<std::string_view> columns;
    std::size_t columnStart = 0;
    for (std::size_t i = 0; i <= row.size(); ++i) {
        if (i < row.size() && row[i] != '|') {
            continue;
        }

        columns.push_back(row.substr(columnStart, i - columnStart));
        columnStart = i + 1;
    }
    return columns;
}

std::optional<CelestialBodyType> parseType(const std::string_view typeText)
{
    const std::string_view normalized = trimAsciiWhitespace(typeText);

    if (normalized == "Star") {
        return CelestialBodyType::Star;
    }

    if (normalized == "Planet") {
        return CelestialBodyType::Planet;
    }

    if (normalized == "Moon") {
        return CelestialBodyType::Moon;
    }

    if (normalized == "Sun") {
        return CelestialBodyType::Sun;
    }

    if (normalized == "Constellation") {
        return CelestialBodyType::Constellation;
    }

    return std::nullopt;
}

std::optional<CelestialBody> parseBodyRow(const std::string_view row)
{
    const std::vector<std::string_view> columns = splitPipeColumns(row);
    if (columns.size() < 4) {
        return std::nullopt;
    }

    const std::string_view id = trimAsciiWhitespace(columns[0]);
    const std::string_view displayName = trimAsciiWhitespace(columns[1]);
    const std::string_view typeText = trimAsciiWhitespace(columns[2]);
    const std::string_view magnitudeText = trimAsciiWhitespace(columns[3]);

    if (id.empty() || displayName.empty()) {
        return std::nullopt;
    }

    const auto type = parseType(typeText);
    if (!type.has_value()) {
        return std::nullopt;
    }

    const auto visualMagnitude = parseFiniteDouble(magnitudeText);
    if (!visualMagnitude.has_value()) {
        return std::nullopt;
    }

    CelestialBody body;
    body.id = std::string(id);
    body.displayName = std::string(displayName);
    body.type = *type;
    body.visualMagnitude = *visualMagnitude;

    if (columns.size() >= 6) {
        const auto rightAscensionHours = parseFiniteDouble(columns[4]);
        const auto declinationDeg = parseFiniteDouble(columns[5]);
        if (rightAscensionHours.has_value() && declinationDeg.has_value()) {
            body.fixedEquatorial = core::EquatorialCoordinate {
                .rightAscensionHours = *rightAscensionHours,
                .declinationDeg = *declinationDeg
            };
        }
    }

    return body;
}

std::vector<CelestialBody> parseBodies(std::string_view catalogRows)
{
    std::vector<CelestialBody> bodies;
    std::size_t cursor = 0;

    while (cursor < catalogRows.size()) {
        const std::size_t newline = catalogRows.find('\n', cursor);
        const std::size_t lineEnd = newline == std::string_view::npos ? catalogRows.size() : newline;
        const std::string_view line = catalogRows.substr(cursor, lineEnd - cursor);
        if (!line.empty() && line[0] != '#') {
            if (const auto body = parseBodyRow(line); body.has_value()) {
                bodies.push_back(*body);
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }

        cursor = newline + 1;
    }

    return bodies;
}

std::vector<std::string> parseCsvColumns(const std::string_view line)
{
    std::vector<std::string> columns;
    std::string currentColumn;
    bool insideQuotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            if (insideQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                currentColumn.push_back('"');
                ++i;
                continue;
            }
            insideQuotes = !insideQuotes;
            continue;
        }

        if (c == ',' && !insideQuotes) {
            columns.push_back(currentColumn);
            currentColumn.clear();
            continue;
        }

        currentColumn.push_back(c);
    }

    columns.push_back(currentColumn);
    return columns;
}

std::vector<CelestialBody> parseBodiesFromHygCsv(std::string_view csvData)
{
    std::vector<CelestialBody> bodies;
    std::size_t cursor = 0;
    bool hasHeader = false;
    std::unordered_map<std::string, std::size_t> headerIndex;

    while (cursor < csvData.size() && bodies.size() < kMaxHygBodyCount) {
        const std::size_t newline = csvData.find('\n', cursor);
        const std::size_t lineEnd = newline == std::string_view::npos ? csvData.size() : newline;
        std::string_view line = csvData.substr(cursor, lineEnd - cursor);
        line = trimAsciiWhitespace(line);

        if (!line.empty()) {
            const auto columns = parseCsvColumns(line);
            if (!hasHeader) {
                for (std::size_t i = 0; i < columns.size(); ++i) {
                    const std::string header = toLowerAscii(trimAsciiWhitespace(columns[i]));
                    if (!header.empty()) {
                        headerIndex[header] = i;
                    }
                }
                hasHeader = true;
            } else {
                const auto findColumn = [&](const char* name) -> std::string_view {
                    const auto it = headerIndex.find(name);
                    if (it == headerIndex.end() || it->second >= columns.size()) {
                        return {};
                    }
                    return trimAsciiWhitespace(columns[it->second]);
                };

                const auto raHours = parseFiniteDouble(findColumn("ra"));
                const auto decDeg = parseFiniteDouble(findColumn("dec"));
                const auto magnitude = parseFiniteDouble(findColumn("mag"));
                if (raHours.has_value() && decDeg.has_value() && magnitude.has_value()) {
                    const std::string_view idField = findColumn("id");
                    const std::string_view properName = findColumn("proper");
                    const std::string_view bayerFlamsteed = findColumn("bf");
                    const std::string_view hip = findColumn("hip");

                    CelestialBody body;
                    body.type = CelestialBodyType::Star;
                    body.visualMagnitude = *magnitude;
                    body.fixedEquatorial = core::EquatorialCoordinate {
                        .rightAscensionHours = *raHours,
                        .declinationDeg = *decDeg
                    };

                    if (!idField.empty()) {
                        body.id = "hyg_" + std::string(idField);
                    } else if (!hip.empty()) {
                        body.id = "hip_" + std::string(hip);
                    } else {
                        body.id = "hyg_auto_" + std::to_string(bodies.size() + 1);
                    }

                    if (!properName.empty()) {
                        body.displayName = std::string(properName);
                    } else if (!bayerFlamsteed.empty()) {
                        body.displayName = std::string(bayerFlamsteed);
                    } else if (!hip.empty()) {
                        body.displayName = "HIP " + std::string(hip);
                    } else {
                        body.displayName = body.id;
                    }

                    bodies.push_back(std::move(body));
                }
            }
        }
        if (newline == std::string_view::npos) {
            break;
        }
        cursor = newline + 1;
    }

    return bodies;
}

std::optional<std::string> gunzipData(const std::string_view gzipData)
{
    if (gzipData.empty()) {
        return std::nullopt;
    }

    z_stream stream {};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(gzipData.data()));
    stream.avail_in = static_cast<uInt>(gzipData.size());

    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        return std::nullopt;
    }

    std::string output;
    output.reserve(gzipData.size() * 3);

    std::array<char, 1 << 14> buffer {};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) {
            inflateEnd(&stream);
            return std::nullopt;
        }

        const std::size_t produced = buffer.size() - stream.avail_out;
        output.append(buffer.data(), produced);
    }

    inflateEnd(&stream);
    if (status != Z_STREAM_END || output.empty()) {
        return std::nullopt;
    }

    return output;
}

}  // namespace

std::unique_ptr<IStarCatalog> createBundledStarCatalog()
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
        "betelgeuse|Betelgeuse|Star|0.50\n"
        "orion|Orion|Constellation|1.6\n"
        "ursa_major|Ursa Major|Constellation|1.8\n"
        "ursa_minor|Ursa Minor|Constellation|2.1\n"
        "cassiopeia|Cassiopeia|Constellation|2.2\n"
        "scorpius|Scorpius|Constellation|1.7\n"
        "cygnus|Cygnus|Constellation|1.3\n"
        "taurus|Taurus|Constellation|1.7\n"
        "leo|Leo|Constellation|1.4\n"
        "gemini|Gemini|Constellation|1.6\n"
        "andromeda|Andromeda|Constellation|2.1\n";

    return std::make_unique<InMemoryStarCatalog>(parseBodies(kCatalogRows));
}

std::unique_ptr<IStarCatalog> createStarCatalogFromRows(const std::string_view catalogRows)
{
    const std::vector<CelestialBody> bodies = parseBodies(catalogRows);
    if (bodies.empty()) {
        return nullptr;
    }

    return std::make_unique<InMemoryStarCatalog>(bodies);
}

std::unique_ptr<IStarCatalog> createStarCatalogFromHygCsv(const std::string_view csvData)
{
    const std::vector<CelestialBody> bodies = parseBodiesFromHygCsv(csvData);
    if (bodies.empty()) {
        return nullptr;
    }

    return std::make_unique<InMemoryStarCatalog>(bodies);
}

std::unique_ptr<IStarCatalog> createStarCatalogFromHygCsvGzip(const std::string_view gzipData)
{
    const auto uncompressedData = gunzipData(gzipData);
    if (!uncompressedData.has_value()) {
        return nullptr;
    }

    return createStarCatalogFromHygCsv(*uncompressedData);
}

}  // namespace skygate::ephemeris
