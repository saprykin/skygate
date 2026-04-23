#include "catalog/BundledCatalogParser.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {
namespace {

struct BundledCatalogEntry final {
    std::string_view id;
    std::string_view displayName;
    CelestialBodyType type;
    double visualMagnitude;
};

struct BundledDeepSkyObjectEntry final {
    std::string_view id;
    std::string_view displayName;
    DeepSkyObjectKind kind;
    double rightAscensionHours;
    double declinationDeg;
    double visualMagnitude;
    double majorAxisArcmin;
    double minorAxisArcmin;
    double positionAngleDeg;
    std::string_view aliases;
};

constexpr std::array<BundledCatalogEntry, 9> kBundledCatalogEntries {{
    {"sun", "Sun", CelestialBodyType::Sun, -26.74},
    {"moon", "Moon", CelestialBodyType::Moon, -12.74},
    {"mercury", "Mercury", CelestialBodyType::Planet, -1.90},
    {"venus", "Venus", CelestialBodyType::Planet, -4.92},
    {"mars", "Mars", CelestialBodyType::Planet, -2.94},
    {"jupiter", "Jupiter", CelestialBodyType::Planet, -2.94},
    {"saturn", "Saturn", CelestialBodyType::Planet, -0.55},
    {"uranus", "Uranus", CelestialBodyType::Planet, 5.68},
    {"neptune", "Neptune", CelestialBodyType::Planet, 7.78},
}};

// Messier subset derived from OpenNGC v20260307. See README attribution.
constexpr std::array<BundledDeepSkyObjectEntry, 110> kBundledMessierEntries {{
    {"messier_001", "M1", DeepSkyObjectKind::Nebula, 5.57554722, 22.01447222, 8.400, 8, 4, 0.0, "M1|NGC 1952|Crab Nebula"},
    {"messier_002", "M2", DeepSkyObjectKind::GlobularCluster, 21.55750278, -0.82330556, 6.250, 8.4, 0.0, 0.0, "M2|NGC 7089"},
    {"messier_003", "M3", DeepSkyObjectKind::GlobularCluster, 13.70311944, 28.37544444, 6.390, 16.2, 0.0, 0.0, "M3|NGC 5272"},
    {"messier_004", "M4", DeepSkyObjectKind::GlobularCluster, 16.39316667, -26.52552778, 5.400, 28.2, 0.0, 0.0, "M4|NGC 6121"},
    {"messier_005", "M5", DeepSkyObjectKind::GlobularCluster, 15.30937500, 2.08269444, 5.950, 15, 0.0, 0.0, "M5|NGC 5904"},
    {"messier_006", "M6", DeepSkyObjectKind::OpenCluster, 17.67243056, -32.25416667, 4.200, 15.6, 0.0, 0.0, "M6|NGC 6405|Butterfly Cluster"},
    {"messier_007", "M7", DeepSkyObjectKind::OpenCluster, 17.89755000, -34.79283333, 3.300, 22.2, 0.0, 0.0, "M7|NGC 6475|Ptolemy's Cluster"},
    {"messier_008", "M8", DeepSkyObjectKind::Nebula, 18.06146389, -24.38016667, 5.800, 45, 30, 0.0, "M8|NGC 6523|NGC 6533|Lagoon Nebula"},
    {"messier_009", "M9", DeepSkyObjectKind::GlobularCluster, 17.31993889, -18.51625000, 8.420, 6.9, 0.0, 0.0, "M9|NGC 6333"},
    {"messier_010", "M10", DeepSkyObjectKind::GlobularCluster, 16.95249722, -4.09933333, 4.980, 9.3, 0.0, 0.0, "M10|NGC 6254"},
    {"messier_011", "M11", DeepSkyObjectKind::OpenCluster, 18.85166389, -6.27002778, 5.800, 9, 0.0, 0.0, "M11|NGC 6705|Amas de l'Ecu de Sobieski|Wild Duck Cluster"},
    {"messier_012", "M12", DeepSkyObjectKind::GlobularCluster, 16.78736667, -1.94783333, 6.070, 11.1, 0.0, 0.0, "M12|NGC 6218"},
    {"messier_013", "M13", DeepSkyObjectKind::GlobularCluster, 16.69489722, 36.46130556, 5.800, 16.5, 0.0, 0.0, "M13|NGC 6205|Hercules Globular Cluster"},
    {"messier_014", "M14", DeepSkyObjectKind::GlobularCluster, 17.62671111, -3.24591667, 5.730, 9.9, 0.0, 0.0, "M14|NGC 6402"},
    {"messier_015", "M15", DeepSkyObjectKind::GlobularCluster, 21.49955000, 12.16683333, 6.300, 11.1, 0.0, 0.0, "M15|NGC 7078"},
    {"messier_016", "M16", DeepSkyObjectKind::Nebula, 18.31338056, -13.80722222, 6.000, 120, 25, 0.0, "M16|NGC 6611|Eagle Nebula"},
    {"messier_017", "M17", DeepSkyObjectKind::Nebula, 18.34641944, -16.17152778, 7.000, 12.6, 0.0, 0.0, "M17|NGC 6618|Checkmark Nebula|Lobster Nebula|Swan Nebula|omega Nebula"},
    {"messier_018", "M18", DeepSkyObjectKind::OpenCluster, 18.33291389, -17.10197222, 6.900, 6, 0.0, 0.0, "M18|NGC 6613"},
    {"messier_019", "M19", DeepSkyObjectKind::GlobularCluster, 17.04380000, -26.26794444, 5.570, 7.5, 0.0, 0.0, "M19|NGC 6273"},
    {"messier_020", "M20", DeepSkyObjectKind::Nebula, 18.04503056, -22.97188889, 8.500, 28, 28, 0.0, "M20|NGC 6514|Trifid Nebula"},
    {"messier_021", "M21", DeepSkyObjectKind::OpenCluster, 18.07040278, -22.49005556, 5.900, 6, 0.0, 0.0, "M21|NGC 6531"},
    {"messier_022", "M22", DeepSkyObjectKind::GlobularCluster, 18.60672222, -23.90341667, 6.170, 12.6, 0.0, 0.0, "M22|NGC 6656"},
    {"messier_023", "M23", DeepSkyObjectKind::OpenCluster, 17.95132500, -18.98533333, 5.500, 16.8, 0.0, 0.0, "M23|NGC 6494"},
    {"messier_024", "M24", DeepSkyObjectKind::Asterism, 18.28225556, -18.51455556, 4.500, 120, 60, 90, "M24|IC 4715|Small Sgr Star Cloud"},
    {"messier_025", "M25", DeepSkyObjectKind::OpenCluster, 18.52965833, -19.11494444, 4.600, 14.1, 0.0, 0.0, "M25|IC 4725"},
    {"messier_026", "M26", DeepSkyObjectKind::OpenCluster, 18.75518333, -9.38361111, 8.870, 6, 0.0, 0.0, "M26|NGC 6694"},
    {"messier_027", "M27", DeepSkyObjectKind::PlanetaryNebula, 19.99343889, 22.72102778, 7.400, 6.7, 0.0, 0.0, "M27|NGC 6853|Dumbbell Nebula"},
    {"messier_028", "M28", DeepSkyObjectKind::GlobularCluster, 18.40913611, -24.86983333, 6.900, 5.1, 0.0, 0.0, "M28|NGC 6626"},
    {"messier_029", "M29", DeepSkyObjectKind::OpenCluster, 20.39938056, 38.50766667, 6.600, 3.6, 0.0, 0.0, "M29|NGC 6913"},
    {"messier_030", "M30", DeepSkyObjectKind::GlobularCluster, 21.67278333, -23.17908333, 7.100, 9, 0.0, 0.0, "M30|NGC 7099"},
    {"messier_031", "M31", DeepSkyObjectKind::Galaxy, 0.71231944, 41.26905556, 3.440, 177.83, 69.66, 35, "M31|NGC 224|Andromeda Galaxy"},
    {"messier_032", "M32", DeepSkyObjectKind::Galaxy, 0.71161944, 40.86527778, 8.130, 7.74, 4.86, 170, "M32|NGC 221"},
    {"messier_033", "M33", DeepSkyObjectKind::Galaxy, 1.56413611, 30.66022222, 5.790, 62.09, 36.73, 23, "M33|NGC 598|Triangulum Galaxy|Triangulum Pinwheel"},
    {"messier_034", "M34", DeepSkyObjectKind::OpenCluster, 2.70205556, 42.74613889, 5.200, 22.5, 0.0, 0.0, "M34|NGC 1039"},
    {"messier_035", "M35", DeepSkyObjectKind::OpenCluster, 6.15140556, 24.33863889, 5.100, 24, 0.0, 0.0, "M35|NGC 2168"},
    {"messier_036", "M36", DeepSkyObjectKind::OpenCluster, 5.60492778, 34.14075000, 6.000, 7.2, 0.0, 0.0, "M36|NGC 1960"},
    {"messier_037", "M37", DeepSkyObjectKind::OpenCluster, 5.87176389, 32.55300000, 5.600, 11.4, 0.0, 0.0, "M37|NGC 2099"},
    {"messier_038", "M38", DeepSkyObjectKind::OpenCluster, 5.47846944, 35.85491667, 6.400, 9.6, 0.0, 0.0, "M38|NGC 1912"},
    {"messier_039", "M39", DeepSkyObjectKind::OpenCluster, 21.53008889, 48.43816667, 4.600, 19.5, 0.0, 0.0, "M39|NGC 7092"},
    {"messier_040", "M40", DeepSkyObjectKind::Unknown, 12.37113889, 58.08444444, 8.000, 0.0, 0.0, 0.0, "M40"},
    {"messier_041", "M41", DeepSkyObjectKind::OpenCluster, 6.76665000, -20.75422222, 4.500, 12, 0.0, 0.0, "M41|NGC 2287"},
    {"messier_042", "M42", DeepSkyObjectKind::Nebula, 5.58791111, -5.38966667, 4.000, 90, 60, 0.0, "M42|NGC 1976|Great Orion Nebula|Orion Nebula"},
    {"messier_043", "M43", DeepSkyObjectKind::Nebula, 5.59205000, -5.26747222, 9.000, 20, 15, 0.0, "M43|NGC 1982|Mairan's Nebula"},
    {"messier_044", "M44", DeepSkyObjectKind::OpenCluster, 8.67283333, 19.67205556, 3.100, 108.6, 0.0, 0.0, "M44|NGC 2632|Beehive|Praesepe Cluster"},
    {"messier_045", "M45", DeepSkyObjectKind::OpenCluster, 3.79127778, 24.10527778, 1.200, 150, 150, 90, "M45|Pleiades"},
    {"messier_046", "M46", DeepSkyObjectKind::OpenCluster, 7.69633889, -14.81000000, 6.100, 21, 0.0, 0.0, "M46|NGC 2437"},
    {"messier_047", "M47", DeepSkyObjectKind::OpenCluster, 7.60972778, -14.48261111, 4.400, 19.8, 0.0, 0.0, "M47|NGC 2422|NGC 2478"},
    {"messier_048", "M48", DeepSkyObjectKind::OpenCluster, 8.22866111, -5.75044444, 5.800, 28.2, 0.0, 0.0, "M48|NGC 2548"},
    {"messier_049", "M49", DeepSkyObjectKind::Galaxy, 12.49632222, 8.00047222, 8.280, 10.21, 8.38, 156, "M49|NGC 4472"},
    {"messier_050", "M50", DeepSkyObjectKind::OpenCluster, 7.04457500, -8.36402778, 5.900, 14.1, 0.0, 0.0, "M50|NGC 2323"},
    {"messier_051", "M51", DeepSkyObjectKind::Galaxy, 13.49797500, 47.19516667, 8.360, 13.71, 11.67, 163, "M51|NGC 5194|Whirlpool Galaxy"},
    {"messier_052", "M52", DeepSkyObjectKind::OpenCluster, 23.41344444, 61.59316667, 6.900, 9.9, 0.0, 0.0, "M52|NGC 7654"},
    {"messier_053", "M53", DeepSkyObjectKind::GlobularCluster, 13.21534167, 18.16911111, 7.790, 9, 0.0, 0.0, "M53|NGC 5024"},
    {"messier_054", "M54", DeepSkyObjectKind::GlobularCluster, 18.91757500, -30.47850000, 7.700, 5.1, 0.0, 0.0, "M54|NGC 6715"},
    {"messier_055", "M55", DeepSkyObjectKind::GlobularCluster, 19.66650000, -30.96208333, 6.490, 12, 0.0, 0.0, "M55|NGC 6809"},
    {"messier_056", "M56", DeepSkyObjectKind::GlobularCluster, 19.27653056, 30.18450000, 8.400, 5.8, 0.0, 0.0, "M56|NGC 6779"},
    {"messier_057", "M57", DeepSkyObjectKind::PlanetaryNebula, 18.89305833, 33.02858333, 8.800, 1.27, 0.0, 0.0, "M57|NGC 6720|Ring Nebula"},
    {"messier_058", "M58", DeepSkyObjectKind::Galaxy, 12.62875556, 11.81819444, 10.300, 5.01, 3.84, 90, "M58|NGC 4579"},
    {"messier_059", "M59", DeepSkyObjectKind::Galaxy, 12.70062222, 11.64702778, 9.560, 4.55, 3.21, 165, "M59|NGC 4621"},
    {"messier_060", "M60", DeepSkyObjectKind::Galaxy, 12.72777222, 11.55269444, 8.790, 6.78, 5.45, 105, "M60|NGC 4649"},
    {"messier_061", "M61", DeepSkyObjectKind::Galaxy, 12.36525000, 4.47363889, 10.250, 6.89, 6.56, 20, "M61|NGC 4303"},
    {"messier_062", "M62", DeepSkyObjectKind::GlobularCluster, 17.02016667, -30.11236111, 7.390, 7.8, 0.0, 0.0, "M62|NGC 6266"},
    {"messier_063", "M63", DeepSkyObjectKind::Galaxy, 13.26370278, 42.02927778, 8.610, 11.83, 7.16, 103, "M63|NGC 5055|Sunflower Galaxy"},
    {"messier_064", "M64", DeepSkyObjectKind::Galaxy, 12.94545556, 21.68297222, 8.520, 10.52, 5.33, 114, "M64|NGC 4826|Black Eye Galaxy|Evil Eye Galaxy"},
    {"messier_065", "M65", DeepSkyObjectKind::Galaxy, 11.31553333, 13.09236111, 9.320, 7.64, 1.97, 173, "M65|NGC 3623"},
    {"messier_066", "M66", DeepSkyObjectKind::Galaxy, 11.33748889, 12.99152778, 8.920, 10.28, 4.61, 168, "M66|NGC 3627"},
    {"messier_067", "M67", DeepSkyObjectKind::OpenCluster, 8.85559167, 11.81194444, 6.900, 33, 0.0, 0.0, "M67|NGC 2682"},
    {"messier_068", "M68", DeepSkyObjectKind::GlobularCluster, 12.65778056, -26.74302778, 7.960, 6.6, 0.0, 0.0, "M68|NGC 4590"},
    {"messier_069", "M69", DeepSkyObjectKind::GlobularCluster, 18.52311944, -32.34797222, 8.310, 5.7, 0.0, 0.0, "M69|NGC 6637|NGC 6634"},
    {"messier_070", "M70", DeepSkyObjectKind::GlobularCluster, 18.72017778, -32.29188889, 9.060, 6.6, 0.0, 0.0, "M70|NGC 6681"},
    {"messier_071", "M71", DeepSkyObjectKind::GlobularCluster, 19.89614167, 18.77838889, 6.100, 6.9, 0.0, 0.0, "M71|NGC 6838|NGC 6839"},
    {"messier_072", "M72", DeepSkyObjectKind::GlobularCluster, 20.89108611, -12.53705556, 8.960, 4.5, 0.0, 0.0, "M72|NGC 6981"},
    {"messier_073", "M73", DeepSkyObjectKind::Unknown, 20.98221389, -12.63550000, 8.900, 0.0, 0.0, 0.0, "M73|NGC 6994"},
    {"messier_074", "M74", DeepSkyObjectKind::Galaxy, 1.61159722, 15.78366667, 9.310, 9.89, 9.33, 87, "M74|NGC 628"},
    {"messier_075", "M75", DeepSkyObjectKind::GlobularCluster, 20.10134444, -21.92222222, 8.260, 3.6, 0.0, 0.0, "M75|NGC 6864"},
    {"messier_076", "M76", DeepSkyObjectKind::PlanetaryNebula, 1.70546944, 51.57547222, 10.100, 1.12, 0.0, 0.0, "M76|NGC 650|NGC 651|Barbell Nebula|Cork Nebula|Little Dumbbell Nebula"},
    {"messier_077", "M77", DeepSkyObjectKind::Galaxy, 2.71130833, -0.01327778, 9.290, 6.11, 5.61, 12, "M77|NGC 1068"},
    {"messier_078", "M78", DeepSkyObjectKind::Nebula, 5.77939444, 0.07930556, 8.000, 4.5, 0.0, 0.0, "M78|NGC 2068"},
    {"messier_079", "M79", DeepSkyObjectKind::GlobularCluster, 5.40294167, -24.52422222, 8.160, 7.2, 0.0, 0.0, "M79|NGC 1904"},
    {"messier_080", "M80", DeepSkyObjectKind::GlobularCluster, 16.28403056, -22.97511111, 7.300, 5.7, 0.0, 0.0, "M80|NGC 6093"},
    {"messier_081", "M81", DeepSkyObjectKind::Galaxy, 9.92588056, 69.06530556, 6.920, 21.63, 11.25, 157, "M81|NGC 3031|Bode's Galaxy"},
    {"messier_082", "M82", DeepSkyObjectKind::Galaxy, 9.93131389, 69.67938889, 8.300, 10.99, 5.11, 66, "M82|NGC 3034|Cigar Galaxy"},
    {"messier_083", "M83", DeepSkyObjectKind::Galaxy, 13.61693056, -29.86541667, 7.210, 13.61, 13.21, 45, "M83|NGC 5236|Southern Pinwheel Galaxy"},
    {"messier_084", "M84", DeepSkyObjectKind::Galaxy, 12.41770556, 12.88697222, 9.790, 7.41, 6.44, 133, "M84|NGC 4374"},
    {"messier_085", "M85", DeepSkyObjectKind::Galaxy, 12.42336389, 18.19150000, 9.050, 6.95, 5.35, 12, "M85|NGC 4382"},
    {"messier_086", "M86", DeepSkyObjectKind::Galaxy, 12.43659444, 12.94622222, 8.860, 11.53, 8.43, 128, "M86|NGC 4406"},
    {"messier_087", "M87", DeepSkyObjectKind::Galaxy, 12.51372778, 12.39111111, 9.000, 7.11, 6.67, 153, "M87|NGC 4486|Virgo Galaxy"},
    {"messier_088", "M88", DeepSkyObjectKind::Galaxy, 12.53310000, 14.42038889, 10.330, 8.65, 4.38, 138, "M88|NGC 4501"},
    {"messier_089", "M89", DeepSkyObjectKind::Galaxy, 12.59439167, 12.55633333, 10.080, 8.13, 8, 150, "M89|NGC 4552"},
    {"messier_090", "M90", DeepSkyObjectKind::Galaxy, 12.61383056, 13.16294444, 9.540, 9.12, 3.82, 22, "M90|NGC 4569"},
    {"messier_091", "M91", DeepSkyObjectKind::Galaxy, 12.59068056, 14.49633333, 10.960, 5.55, 4.52, 150, "M91|NGC 4548"},
    {"messier_092", "M92", DeepSkyObjectKind::GlobularCluster, 17.28535278, 43.13652778, 6.520, 14.4, 0.0, 0.0, "M92|NGC 6341"},
    {"messier_093", "M93", DeepSkyObjectKind::OpenCluster, 7.74145278, -23.85308333, 6.200, 15, 0.0, 0.0, "M93|NGC 2447"},
    {"messier_094", "M94", DeepSkyObjectKind::Galaxy, 12.84807222, 41.12044444, 8.240, 7.74, 6.68, 105, "M94|NGC 4736"},
    {"messier_095", "M95", DeepSkyObjectKind::Galaxy, 10.73269444, 11.70380556, 9.770, 7.23, 4.45, 11, "M95|NGC 3351"},
    {"messier_096", "M96", DeepSkyObjectKind::Galaxy, 10.77937222, 11.81994444, 9.210, 8.26, 5.51, 5, "M96|NGC 3368"},
    {"messier_097", "M97", DeepSkyObjectKind::PlanetaryNebula, 11.24658611, 55.01902778, 9.900, 3.58, 0.0, 0.0, "M97|NGC 3587|Owl Nebula"},
    {"messier_098", "M98", DeepSkyObjectKind::Galaxy, 12.23008056, 14.90033333, 10.840, 11.04, 2.66, 152, "M98|NGC 4192"},
    {"messier_099", "M99", DeepSkyObjectKind::Galaxy, 12.31377778, 14.41650000, 9.840, 5.04, 4.74, 23, "M99|NGC 4254|Coma Pinwheel|Virgo Cluster Pinwheel"},
    {"messier_100", "M100", DeepSkyObjectKind::Galaxy, 12.38189722, 15.82180556, 9.470, 6.1, 5.62, 108, "M100|NGC 4321"},
    {"messier_101", "M101", DeepSkyObjectKind::Galaxy, 14.05348333, 54.34894444, 7.900, 23.99, 23.07, 28, "M101|NGC 5457"},
    {"messier_102", "M102", DeepSkyObjectKind::Galaxy, 15.10819444, 55.76333333, 9.900, 6.31, 3.02, 128, "M102|NGC 5866|Spindle Galaxy"},
    {"messier_103", "M103", DeepSkyObjectKind::OpenCluster, 1.55605833, 60.65800000, 7.400, 4.5, 0.0, 0.0, "M103|NGC 581"},
    {"messier_104", "M104", DeepSkyObjectKind::Galaxy, 12.66650833, -11.62305556, 8.590, 8.45, 4.91, 90, "M104|NGC 4594|Sombrero Galaxy"},
    {"messier_105", "M105", DeepSkyObjectKind::Galaxy, 10.79710833, 12.58161111, 9.270, 4.89, 4.25, 71, "M105|NGC 3379"},
    {"messier_106", "M106", DeepSkyObjectKind::Galaxy, 12.31597222, 47.30397222, 9.290, 16.98, 7.24, 150, "M106|NGC 4258"},
    {"messier_107", "M107", DeepSkyObjectKind::GlobularCluster, 16.54220000, -13.05363889, 8.850, 7.8, 0.0, 0.0, "M107|NGC 6171"},
    {"messier_108", "M108", DeepSkyObjectKind::Galaxy, 11.19193611, 55.67411111, 10.050, 3.98, 1.66, 79, "M108|NGC 3556"},
    {"messier_109", "M109", DeepSkyObjectKind::Galaxy, 11.95999444, 53.37452778, 9.880, 8.07, 5.64, 78, "M109|NGC 3992"},
    {"messier_110", "M110", DeepSkyObjectKind::Galaxy, 0.67280000, 41.68530556, 8.150, 16.22, 9.59, 170, "M110|NGC 205"},
}};

std::vector<std::string> splitAliases(std::string_view aliases)
{
    std::vector<std::string> values;
    std::size_t cursor = 0;
    while (cursor <= aliases.size()) {
        const std::size_t delimiter = aliases.find('|', cursor);
        const std::size_t tokenEnd = delimiter == std::string_view::npos
            ? aliases.size()
            : delimiter;
        const std::string_view token = aliases.substr(cursor, tokenEnd - cursor);
        if (!token.empty()) {
            values.emplace_back(token);
        }
        if (delimiter == std::string_view::npos) {
            break;
        }
        cursor = delimiter + 1;
    }
    return values;
}

std::optional<double> positiveOptional(const double value)
{
    if (value <= 0.0) {
        return std::nullopt;
    }
    return value;
}

}  // namespace

CatalogBodyParseResult BundledCatalogParser::parse() const
{
    CatalogBodyParseResult result;
    result.bodies.reserve(kBundledCatalogEntries.size() + kBundledMessierEntries.size());
    for (const auto& entry : kBundledCatalogEntries) {
        result.bodies.push_back(CelestialBody {
            .id = std::string(entry.id),
            .displayName = std::string(entry.displayName),
            .type = entry.type,
            .visualMagnitude = entry.visualMagnitude,
        });
    }

    for (const auto& entry : kBundledMessierEntries) {
        result.bodies.push_back(CelestialBody {
            .id = std::string(entry.id),
            .displayName = std::string(entry.displayName),
            .type = CelestialBodyType::DeepSkyObject,
            .visualMagnitude = entry.visualMagnitude,
            .fixedEquatorial = core::EquatorialCoordinate {
                .rightAscensionHours = entry.rightAscensionHours,
                .declinationDeg = entry.declinationDeg
            },
            .deepSkyObject = DeepSkyObjectInfo {
                .kind = entry.kind,
                .aliases = splitAliases(entry.aliases),
                .majorAxisArcmin = positiveOptional(entry.majorAxisArcmin),
                .minorAxisArcmin = positiveOptional(entry.minorAxisArcmin),
                .positionAngleDeg = positiveOptional(entry.positionAngleDeg),
            }
        });
    }

    result.diagnostics.processedRowCount = result.bodies.size();
    result.diagnostics.parsedBodyCount = result.bodies.size();
    return result;
}

}  // namespace skygate::ephemeris
