#pragma once

namespace skygate::core {

class NumericTolerance final {
public:
    // Differences below this threshold are treated as unchanged when deciding
    // whether a display-facing value needs a new frame or a new sort order.
    static constexpr double kDisplayChange = 1e-9;
};

}  // namespace skygate::core
