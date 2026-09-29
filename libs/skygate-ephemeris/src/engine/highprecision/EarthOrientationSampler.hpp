#pragma once

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

        AstronomicalEpoch requestedUtcEpoch;
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
            warningCodeMask |= warningMask(code);
        }

        [[nodiscard]] bool hasWarning(const WarningCode code) const noexcept
        {
            return (warningCodeMask & warningMask(code)) != 0U;
        }

    private:
        [[nodiscard]] static constexpr std::uint32_t warningMask(const WarningCode code) noexcept
        {
            return 1U << static_cast<std::uint8_t>(code);
        }
    };

    [[nodiscard]] static Sample sample(const IEarthOrientationProvider* provider, const AstronomicalEpoch& utcEpoch);
    [[nodiscard]] static Sample
    sample(const IEarthOrientationProvider* provider, const AstronomicalEpoch& utcEpoch, const Options& options);
    [[nodiscard]] static Sample
    sample(const std::shared_ptr<const IEarthOrientationProvider>& provider, const AstronomicalEpoch& utcEpoch);
    [[nodiscard]] static Sample sample(
        const std::shared_ptr<const IEarthOrientationProvider>& provider,
        const AstronomicalEpoch& utcEpoch,
        const Options& options
    );
};

}  // namespace skygate::ephemeris
