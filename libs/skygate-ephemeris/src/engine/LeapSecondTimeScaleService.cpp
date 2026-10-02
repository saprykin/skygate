#include "LeapSecondTimeScaleService.hpp"
#include "TdbTtConverter.hpp"
#include "math/TimeConstants.hpp"
#include "time/CalendarTime.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace skygate::ephemeris {

namespace {

using skygate::core::TimeConstants;

[[nodiscard]] TimeScaleConversionResult successResult(
    skygate::core::AstronomicalEpoch epoch,
    const TimeScaleConversionStatus status = TimeScaleConversionStatus::Valid,
    const std::uint32_t warningCodeMask = 0U,
    std::string diagnosticText = {}
)
{
    if (diagnosticText.empty()) {
        diagnosticText = status == TimeScaleConversionStatus::Valid ? "Time-scale conversion succeeded."
                                                                    : "Time-scale conversion degraded.";
    }

    return TimeScaleConversionResult{
        .epoch = epoch.normalized(),
        .status = status,
        .warningCodeMask = warningCodeMask,
        .diagnosticText = std::move(diagnosticText),
    };
}

[[nodiscard]] TimeScaleConversionResult failureResult(
    const skygate::core::AstronomicalEpoch& epoch,
    const skygate::core::TimeScale targetScale,
    std::string diagnosticText
)
{
    TimeScaleConversionResult result;
    result.epoch = epoch;
    result.epoch.timeScale = targetScale;
    result.status = TimeScaleConversionStatus::Failed;
    result.diagnosticText = std::move(diagnosticText);
    return result;
}

[[nodiscard]] bool isUtcLeapSecondLabel(const skygate::core::CivilDateTime& dateTime) noexcept
{
    return dateTime.timeScale == skygate::core::TimeScale::Utc && dateTime.hour == 23 && dateTime.minute == 59
           && dateTime.second == 60;
}

void mergeConversionWarnings(TimeScaleConversionResult& result, const TimeScaleConversionResult& source) noexcept
{
    result.warningCodeMask |= source.warningCodeMask;
    if (source.status == TimeScaleConversionStatus::Degraded && result.status == TimeScaleConversionStatus::Valid) {
        result.status = TimeScaleConversionStatus::Degraded;
    }
}

}  // namespace

LeapSecondTimeScaleService::LeapSecondTimeScaleService(
    std::shared_ptr<const ILeapSecondProvider> leapSecondProvider,
    TimeScaleServiceOptions options,
    std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider,
    std::shared_ptr<const IDeltaTProvider> deltaTProvider
)
    : m_leapSecondOffsetResolver(leapSecondProvider, options),
      m_ut1OffsetResolver(leapSecondProvider, earthOrientationProvider, deltaTProvider, options)
{
}

TimeScaleConversionResult LeapSecondTimeScaleService::convert(
    const skygate::core::AstronomicalEpoch& epoch, const skygate::core::TimeScale targetScale
) const
{
    if (!epoch.isFinite()) {
        TimeScaleConversionResult result = failureResult(epoch, targetScale, "Time-scale conversion input is invalid.");
        result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }

    if (epoch.timeScale == targetScale) {
        return successResult(epoch);
    }

    switch (epoch.timeScale) {
    case skygate::core::TimeScale::Utc: {
        if (targetScale == skygate::core::TimeScale::Ut1) {
            const Ut1OffsetResolver::Result lookup = m_ut1OffsetResolver.lookupUtcToUt1Offset(epoch);
            if (!lookup.ut1MinusUtcSeconds.has_value()) {
                TimeScaleConversionResult result = failureResult(epoch, targetScale, lookup.diagnosticText);
                result.warningCodeMask = lookup.warningCodeMask;
                return result;
            }

            TimeScaleConversionResult result =
                successResult(epoch.addSeconds(*lookup.ut1MinusUtcSeconds, skygate::core::TimeScale::Ut1));
            Ut1OffsetResolver::mergeWarnings(result, lookup);
            if (!lookup.diagnosticText.empty()) {
                result.diagnosticText = lookup.diagnosticText;
            }
            return result;
        }

        if (targetScale == skygate::core::TimeScale::Tdb) {
            const TimeScaleConversionResult tt = convert(epoch, skygate::core::TimeScale::Tt);
            if (!tt.isSuccess()) {
                TimeScaleConversionResult result = tt;
                result.epoch.timeScale = targetScale;
                return result;
            }

            TimeScaleConversionResult tdb = convert(tt.epoch, skygate::core::TimeScale::Tdb);
            mergeConversionWarnings(tdb, tt);
            return tdb;
        }

        if (targetScale != skygate::core::TimeScale::Tai && targetScale != skygate::core::TimeScale::Tt) {
            TimeScaleConversionResult result =
                failureResult(epoch, targetScale, "Only UTC to TAI, TT, or TDB conversion is currently supported.");
            result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
            return result;
        }

        const LeapSecondOffsetResolver::Result lookup = m_leapSecondOffsetResolver.lookupUtcOffset(epoch);
        if (!lookup.offsetSeconds.has_value()) {
            TimeScaleConversionResult result = failureResult(epoch, targetScale, lookup.diagnosticText);
            result.warningCodeMask = lookup.warningCodeMask;
            return result;
        }

        const double taiOffset = static_cast<double>(*lookup.offsetSeconds);
        const double targetOffset =
            targetScale == skygate::core::TimeScale::Tt ? taiOffset + TimeConstants::kTtMinusTaiSeconds : taiOffset;
        TimeScaleConversionResult result =
            successResult(epoch.addSeconds(targetOffset, targetScale), lookup.status, lookup.warningCodeMask);
        if (!lookup.diagnosticText.empty()) {
            result.diagnosticText = lookup.diagnosticText;
        }
        return result;
    }
    case skygate::core::TimeScale::Tai:
        if (targetScale == skygate::core::TimeScale::Tdb) {
            TimeScaleConversionResult tt = convert(epoch, skygate::core::TimeScale::Tt);
            if (!tt.isSuccess()) {
                TimeScaleConversionResult result = tt;
                result.epoch.timeScale = targetScale;
                return result;
            }

            TimeScaleConversionResult tdb = convert(tt.epoch, skygate::core::TimeScale::Tdb);
            mergeConversionWarnings(tdb, tt);
            return tdb;
        }
        if (targetScale == skygate::core::TimeScale::Tt) {
            return successResult(epoch.addSeconds(TimeConstants::kTtMinusTaiSeconds, skygate::core::TimeScale::Tt));
        }
        if (targetScale == skygate::core::TimeScale::Utc) {
            const LeapSecondOffsetResolver::Result lookup = m_leapSecondOffsetResolver.lookupTaiOffset(epoch);
            if (!lookup.offsetSeconds.has_value()) {
                TimeScaleConversionResult result = failureResult(epoch, targetScale, lookup.diagnosticText);
                result.warningCodeMask = lookup.warningCodeMask;
                return result;
            }

            TimeScaleConversionResult result = successResult(
                epoch.addSeconds(-static_cast<double>(*lookup.offsetSeconds), skygate::core::TimeScale::Utc)
            );
            LeapSecondOffsetResolver::mergeWarnings(result, lookup);
            if (!lookup.diagnosticText.empty()) {
                result.diagnosticText = lookup.diagnosticText;
            }
            return result;
        }
        if (targetScale == skygate::core::TimeScale::Ut1) {
            TimeScaleConversionResult utc = convert(epoch, skygate::core::TimeScale::Utc);
            if (!utc.isSuccess()) {
                utc.epoch.timeScale = targetScale;
                return utc;
            }

            TimeScaleConversionResult ut1 = convert(utc.epoch, skygate::core::TimeScale::Ut1);
            mergeConversionWarnings(ut1, utc);
            return ut1;
        }
        break;
    case skygate::core::TimeScale::Tt:
        if (targetScale == skygate::core::TimeScale::Ut1) {
            TimeScaleConversionResult utc = convert(epoch, skygate::core::TimeScale::Utc);
            if (!utc.isSuccess()) {
                utc.epoch.timeScale = targetScale;
                return utc;
            }

            TimeScaleConversionResult ut1 = convert(utc.epoch, skygate::core::TimeScale::Ut1);
            mergeConversionWarnings(ut1, utc);
            return ut1;
        }
        if (targetScale == skygate::core::TimeScale::Tdb) {
            return TdbTtConverter::ttToTdb(epoch);
        }
        if (targetScale == skygate::core::TimeScale::Tai) {
            return successResult(epoch.addSeconds(-TimeConstants::kTtMinusTaiSeconds, skygate::core::TimeScale::Tai));
        }
        if (targetScale == skygate::core::TimeScale::Utc) {
            return convert(
                epoch.addSeconds(-TimeConstants::kTtMinusTaiSeconds, skygate::core::TimeScale::Tai),
                skygate::core::TimeScale::Utc
            );
        }
        break;
    case skygate::core::TimeScale::Tdb: {
        TimeScaleConversionResult result = TdbTtConverter::tdbToTt(epoch);
        if (targetScale == skygate::core::TimeScale::Tt) {
            return result;
        }

        TimeScaleConversionResult converted = convert(result.epoch, targetScale);
        mergeConversionWarnings(converted, result);
        return converted;
    }
    case skygate::core::TimeScale::Ut1: {
        TimeScaleConversionResult utc = m_ut1OffsetResolver.lookupUtcFromUt1(epoch);
        if (targetScale == skygate::core::TimeScale::Utc) {
            return utc;
        }
        if (!utc.isSuccess()) {
            utc.epoch.timeScale = targetScale;
            return utc;
        }

        TimeScaleConversionResult converted = convert(utc.epoch, targetScale);
        mergeConversionWarnings(converted, utc);
        return converted;
    }
    }

    TimeScaleConversionResult result =
        failureResult(epoch, targetScale, "The requested time-scale conversion is not supported.");
    result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
    return result;
}

TimeScaleConversionResult LeapSecondTimeScaleService::convertCivilDateTime(
    const skygate::core::CivilDateTime& dateTime, const skygate::core::TimeScale targetScale
) const
{
    if (!skygate::core::CalendarTime::isValidCivilDateTime(dateTime)) {
        skygate::core::AstronomicalEpoch epoch;
        epoch.timeScale = targetScale;
        TimeScaleConversionResult result = failureResult(epoch, targetScale, "Civil date/time input is invalid.");
        result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }

    std::optional<skygate::core::AstronomicalEpoch> epoch;
    if (!isUtcLeapSecondLabel(dateTime)) {
        epoch = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(dateTime);
    } else {
        skygate::core::CivilDateTime precedingSecond = dateTime;
        precedingSecond.second = 59;
        const std::optional<skygate::core::AstronomicalEpoch> precedingEpoch =
            skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(precedingSecond);
        if (precedingEpoch.has_value()) {
            epoch = precedingEpoch->addSeconds(1.0, skygate::core::TimeScale::Utc);
        }
    }

    if (!epoch.has_value()) {
        skygate::core::AstronomicalEpoch failedEpoch;
        failedEpoch.timeScale = targetScale;
        TimeScaleConversionResult result =
            failureResult(failedEpoch, targetScale, "Civil date/time input could not be converted to an epoch.");
        result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }

    if (!isUtcLeapSecondLabel(dateTime)) {
        return convert(*epoch, targetScale);
    }

    skygate::core::CivilDateTime precedingSecond = dateTime;
    precedingSecond.second = 59;
    const std::optional<skygate::core::AstronomicalEpoch> precedingEpoch =
        skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(precedingSecond);
    if (!precedingEpoch.has_value()) {
        TimeScaleConversionResult result =
            failureResult(*epoch, targetScale, "Leap-second civil date/time input is invalid.");
        result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }

    const LeapSecondOffsetResolver::Result precedingLookup =
        m_leapSecondOffsetResolver.lookupUtcOffset(*precedingEpoch);
    const LeapSecondOffsetResolver::Result nextLookup = m_leapSecondOffsetResolver.lookupUtcOffset(*epoch);
    if (!precedingLookup.offsetSeconds.has_value() || !nextLookup.offsetSeconds.has_value()
        || *nextLookup.offsetSeconds != *precedingLookup.offsetSeconds + 1) {
        TimeScaleConversionResult result =
            failureResult(*epoch, targetScale, "Civil date/time does not identify a configured positive leap second.");
        result.warningCodeMask = precedingLookup.warningCodeMask | nextLookup.warningCodeMask;
        result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }

    if (targetScale == skygate::core::TimeScale::Utc) {
        return successResult(*epoch);
    }
    if (targetScale != skygate::core::TimeScale::Tai && targetScale != skygate::core::TimeScale::Tt
        && targetScale != skygate::core::TimeScale::Tdb) {
        TimeScaleConversionResult result = failureResult(
            *epoch, targetScale, "Only UTC leap-second conversion to TAI, TT, or TDB is currently supported."
        );
        result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
        return result;
    }

    const double taiOffset = static_cast<double>(*precedingLookup.offsetSeconds);
    const double targetOffset =
        targetScale == skygate::core::TimeScale::Tai ? taiOffset : taiOffset + TimeConstants::kTtMinusTaiSeconds;
    TimeScaleConversionResult result = successResult(epoch->addSeconds(
        targetOffset, targetScale == skygate::core::TimeScale::Tdb ? skygate::core::TimeScale::Tt : targetScale
    ));
    LeapSecondOffsetResolver::mergeWarnings(result, precedingLookup);
    LeapSecondOffsetResolver::mergeWarnings(result, nextLookup);
    if (targetScale == skygate::core::TimeScale::Tdb) {
        TimeScaleConversionResult tdb = convert(result.epoch, skygate::core::TimeScale::Tdb);
        mergeConversionWarnings(tdb, result);
        return tdb;
    }
    return result;
}

}  // namespace skygate::ephemeris
