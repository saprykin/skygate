#pragma once

#include "BitFlagSetOperations.hpp"
#include "IEarthOrientationProvider.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace skygate::ephemeris {

class EarthOrientationSampler final {
public:
    struct Options {
        bool allowOutOfRangeNearestSampleFallback = true;
        bool allowMissingDataZeroFallback = false;
        bool degradePredictedData = true;

        // Fallback profile for callers that prefer a degraded sample over an
        // unavailable Earth-orientation correction.
        [[nodiscard]] static Options tolerantFallback() noexcept
        {
            Options options;
            options.allowMissingDataZeroFallback = true;
            options.degradePredictedData = false;
            return options;
        }
    };

    struct Sample {
        enum class Status : std::uint8_t {
            Valid,
            Degraded,
            Failed
        };

        enum class WarningCode : std::uint8_t {
            StaleData,
            PredictedData,
            MissingData,
            EpochOutsideRange,
            InvalidInput,
            EstimatedData
        };

        skygate::core::AstronomicalEpoch requestedUtcEpoch;
        double ut1MinusUtcSeconds = 0.0;
        double polarMotionXArcseconds = 0.0;
        double polarMotionYArcseconds = 0.0;
        bool predicted = false;
        bool estimated = false;
        Status status = Status::Failed;
        std::uint32_t warningCodeMask = 0U;
        std::string diagnosticText;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return status == Status::Valid || status == Status::Degraded;
        }

        void addWarning(const WarningCode code) noexcept
        {
            warningCodeMask |= BitFlagSetOperations::bit(code);
        }

        [[nodiscard]] bool hasWarning(const WarningCode code) const noexcept
        {
            return (warningCodeMask & BitFlagSetOperations::bit(code)) != 0U;
        }
    };

    [[nodiscard]] static Sample
    sample(const IEarthOrientationProvider* provider, const skygate::core::AstronomicalEpoch& utcEpoch);
    [[nodiscard]] static Sample sample(
        const IEarthOrientationProvider* provider,
        const skygate::core::AstronomicalEpoch& utcEpoch,
        const Options& options
    );
    [[nodiscard]] static Sample sample(
        const std::shared_ptr<const IEarthOrientationProvider>& provider,
        const skygate::core::AstronomicalEpoch& utcEpoch
    );
    [[nodiscard]] static Sample sample(
        const std::shared_ptr<const IEarthOrientationProvider>& provider,
        const skygate::core::AstronomicalEpoch& utcEpoch,
        const Options& options
    );
};

}  // namespace skygate::ephemeris
