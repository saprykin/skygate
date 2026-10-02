#pragma once

namespace skygate::core {

class NaifBodyIds final {
public:
    static constexpr int kSolarSystemBarycenter = 0;
    static constexpr int kMercuryBarycenter = 1;
    static constexpr int kVenusBarycenter = 2;
    static constexpr int kEarthMoonBarycenter = 3;
    static constexpr int kMarsBarycenter = 4;
    static constexpr int kJupiterBarycenter = 5;
    static constexpr int kSaturnBarycenter = 6;
    static constexpr int kUranusBarycenter = 7;
    static constexpr int kNeptuneBarycenter = 8;
    static constexpr int kPlutoBarycenter = 9;
    static constexpr int kSun = 10;
    static constexpr int kMercury = 199;
    static constexpr int kVenus = 299;
    static constexpr int kMoon = 301;
    static constexpr int kEarth = 399;
    static constexpr int kMars = 499;
    static constexpr int kJupiter = 599;
    static constexpr int kSaturn = 699;
    static constexpr int kUranus = 799;
    static constexpr int kNeptune = 899;
    static constexpr int kPluto = 999;
};

}  // namespace skygate::core
