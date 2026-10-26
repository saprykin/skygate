#pragma once

#include "BitFlagSet.hpp"
#include "EphemerisEngineWarning.hpp"
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

        // How the sampler obtained the values for the requested epoch.
        enum class Origin : std::uint8_t {
            ExactRow,
            Interpolated,
            NearestRow,
            ZeroFallback
        };

        enum class WarningCode : std::uint8_t {
            StaleData,
            PredictedData,
            MissingData,
            EpochOutsideRange,
            InvalidInput,
            EstimatedData
        };

        using WarningMask = BitFlagSet<WarningCode>;

        skygate::core::AstronomicalEpoch requestedUtcEpoch;
        double ut1MinusUtcSeconds = 0.0;
        double polarMotionXArcseconds = 0.0;
        double polarMotionYArcseconds = 0.0;
        bool predicted = false;
        bool estimated = false;
        Status status = Status::Failed;
        // Meaningful for successful samples; failed samples never used table
        // values.
        Origin origin = Origin::ZeroFallback;
        WarningMask warningCodeMask;
        std::string diagnosticText;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return status == Status::Valid || status == Status::Degraded;
        }

        void addWarning(const WarningCode code) noexcept
        {
            warningCodeMask.add(WarningMask::bit(code));
        }

        [[nodiscard]] bool hasWarning(const WarningCode code) const noexcept
        {
            return warningCodeMask.has(WarningMask::bit(code));
        }

        // Sentence that describes how the values were obtained. Row origins
        // add a degradation suffix when the status is degraded, while the
        // fallback origins already name the degradation in their wording.
        // `nearestRowUsedFirstRow` only selects the wording of an out-of-range
        // fallback and is ignored for the other origins.
        [[nodiscard]] static std::string diagnosticTextFor(Origin origin, Status status, bool nearestRowUsedFirstRow);

        [[nodiscard]] static EphemerisEngineWarning::Code engineWarningCode(WarningCode code) noexcept;
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
