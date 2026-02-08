#include "skygate/ephemeris/ConstellationData.hpp"

#include <array>
#include <string_view>
#include <utility>

namespace skygate::ephemeris {
namespace {

struct ConstellationLineIdRef {
    std::string_view startId;
    std::string_view endId;
};

constexpr std::array<ConstellationLineIdRef, 57> kBundledConstellationLineRefs = {{
    // Bundled fallback.
    {.startId = "sirius", .endId = "procyon"},
    {.startId = "procyon", .endId = "betelgeuse"},
    {.startId = "betelgeuse", .endId = "rigel"},
    {.startId = "rigel", .endId = "sirius"},
    {.startId = "betelgeuse", .endId = "capella"},
    {.startId = "capella", .endId = "vega"},
    {.startId = "vega", .endId = "arcturus"},
    // Orion.
    {.startId = "hip_27989", .endId = "hip_25336"},
    {.startId = "hip_27989", .endId = "hip_26727"},
    {.startId = "hip_25336", .endId = "hip_25930"},
    {.startId = "hip_25930", .endId = "hip_26311"},
    {.startId = "hip_26311", .endId = "hip_26727"},
    {.startId = "hip_26727", .endId = "hip_27366"},
    {.startId = "hip_27366", .endId = "hip_24436"},
    {.startId = "hip_24436", .endId = "hip_25930"},
    {.startId = "hip_26207", .endId = "hip_25930"},
    {.startId = "hip_26207", .endId = "hip_27989"},
    // Ursa Major.
    {.startId = "hip_67301", .endId = "hip_65378"},
    {.startId = "hip_65378", .endId = "hip_62956"},
    {.startId = "hip_62956", .endId = "hip_59774"},
    {.startId = "hip_59774", .endId = "hip_54061"},
    {.startId = "hip_54061", .endId = "hip_53910"},
    {.startId = "hip_53910", .endId = "hip_58001"},
    // Ursa Minor.
    {.startId = "hip_11767", .endId = "hip_79822"},
    {.startId = "hip_79822", .endId = "hip_77055"},
    {.startId = "hip_77055", .endId = "hip_75097"},
    {.startId = "hip_75097", .endId = "hip_72607"},
    {.startId = "hip_72607", .endId = "hip_85822"},
    {.startId = "hip_85822", .endId = "hip_82080"},
    {.startId = "hip_82080", .endId = "hip_11767"},
    // Cassiopeia.
    {.startId = "hip_746", .endId = "hip_3179"},
    {.startId = "hip_3179", .endId = "hip_4427"},
    {.startId = "hip_4427", .endId = "hip_6686"},
    {.startId = "hip_6686", .endId = "hip_8886"},
    // Cygnus.
    {.startId = "hip_102098", .endId = "hip_100453"},
    {.startId = "hip_100453", .endId = "hip_95947"},
    {.startId = "hip_100453", .endId = "hip_98110"},
    {.startId = "hip_100453", .endId = "hip_97165"},
    {.startId = "hip_98110", .endId = "hip_95947"},
    // Taurus.
    {.startId = "hip_20889", .endId = "hip_21421"},
    {.startId = "hip_20889", .endId = "hip_25428"},
    {.startId = "hip_21421", .endId = "hip_25428"},
    // Gemini.
    {.startId = "hip_31681", .endId = "hip_34088"},
    {.startId = "hip_34088", .endId = "hip_35550"},
    {.startId = "hip_37826", .endId = "hip_34088"},
    {.startId = "hip_34088", .endId = "hip_32362"},
    // Leo.
    {.startId = "hip_49669", .endId = "hip_54872"},
    {.startId = "hip_54872", .endId = "hip_57632"},
    {.startId = "hip_54872", .endId = "hip_50583"},
    // Andromeda.
    {.startId = "hip_677", .endId = "hip_3092"},
    {.startId = "hip_3092", .endId = "hip_5447"},
    {.startId = "hip_5447", .endId = "hip_9640"},
    // Scorpius.
    {.startId = "hip_78265", .endId = "hip_78401"},
    {.startId = "hip_78401", .endId = "hip_78820"},
    {.startId = "hip_78820", .endId = "hip_80763"},
    {.startId = "hip_80763", .endId = "hip_85927"},
    {.startId = "hip_85927", .endId = "hip_86228"},
}};

}  // namespace

std::vector<ConstellationLineRef> BundledConstellationData::lineRefs() const
{
    std::vector<ConstellationLineRef> refs;
    refs.reserve(kBundledConstellationLineRefs.size());
    for (const auto& lineRef : kBundledConstellationLineRefs) {
        refs.emplace_back(std::string(lineRef.startId), std::string(lineRef.endId));
    }
    return refs;
}

std::vector<ConstellationLabelRef> BundledConstellationData::labelRefs() const
{
    return {
        {"Orion", {"hip_27989", "hip_25336", "hip_25930", "hip_26311", "hip_26727", "hip_24436"}},
        {"Ursa Major", {"hip_67301", "hip_65378", "hip_62956", "hip_59774", "hip_54061", "hip_53910", "hip_58001"}},
        {"Ursa Minor", {"hip_11767", "hip_79822", "hip_77055", "hip_75097", "hip_72607", "hip_85822", "hip_82080"}},
        {"Cassiopeia", {"hip_746", "hip_3179", "hip_4427", "hip_6686", "hip_8886"}},
        {"Cygnus", {"hip_102098", "hip_100453", "hip_95947", "hip_98110", "hip_97165"}},
        {"Taurus", {"hip_20889", "hip_21421", "hip_25428"}},
        {"Gemini", {"hip_31681", "hip_34088", "hip_35550", "hip_37826", "hip_32362"}},
        {"Leo", {"hip_49669", "hip_54872", "hip_57632", "hip_50583"}},
        {"Andromeda", {"hip_677", "hip_3092", "hip_5447", "hip_9640"}},
        {"Scorpius", {"hip_78265", "hip_78401", "hip_78820", "hip_80763", "hip_85927", "hip_86228"}},
    };
}

}  // namespace skygate::ephemeris
