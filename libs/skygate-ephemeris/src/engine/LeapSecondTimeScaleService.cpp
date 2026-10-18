#include "LeapSecondTimeScaleService.hpp"
#include "TdbTtConverter.hpp"
#include "math/TimeConstants.hpp"
#include "time/CalendarTime.hpp"

#include <optional>
#include <string>
#include <utility>

namespace skygate::ephemeris {

using skygate::core::TimeConstants;

double LeapSecondTimeScaleService::targetOffsetSeconds(
    const skygate::core::TimeScale target, const double taiMinusUtcSeconds
) noexcept
{
    // TT and TDB conversions add the fixed TT-TAI offset; TAI conversion does not.
    if (target == skygate::core::TimeScale::Tai) {
        return taiMinusUtcSeconds;
    }
    return taiMinusUtcSeconds + TimeConstants::kTtMinusTaiSeconds;
}

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
        TimeScaleConversionResult result =
            TimeScaleConversionResult::failure(epoch, targetScale, "Time-scale conversion input is invalid.");
        result.addWarning(TimeScaleConversionWarningCode::Type::InvalidInput);
        return result;
    }

    if (epoch.timeScale == targetScale) {
        return TimeScaleConversionResult::success(epoch);
    }

    switch (epoch.timeScale) {
    case skygate::core::TimeScale::Utc: {
        if (targetScale == skygate::core::TimeScale::Ut1) {
            const Ut1OffsetResolver::Result lookup = m_ut1OffsetResolver.lookupUtcToUt1Offset(epoch);
            if (!lookup.ut1MinusUtcSeconds.has_value()) {
                return TimeScaleConversionResult::failureFromLookup(
                    epoch, targetScale, lookup.warningCodeMask, lookup.diagnosticText
                );
            }

            TimeScaleConversionResult result = TimeScaleConversionResult::success(
                epoch.addSeconds(*lookup.ut1MinusUtcSeconds, skygate::core::TimeScale::Ut1)
            );
            applyLookup(result, lookup);
            return result;
        }

        if (targetScale == skygate::core::TimeScale::Tdb) {
            return convertVia(skygate::core::TimeScale::Tt, epoch, targetScale);
        }

        if (targetScale != skygate::core::TimeScale::Tai && targetScale != skygate::core::TimeScale::Tt) {
            TimeScaleConversionResult result = TimeScaleConversionResult::failure(
                epoch, targetScale, "Only UTC to TAI, TT, or TDB conversion is currently supported."
            );
            result.addWarning(TimeScaleConversionWarningCode::Type::UnsupportedConversion);
            return result;
        }

        const LeapSecondOffsetResolver::Result lookup = m_leapSecondOffsetResolver.lookupUtcOffset(epoch);
        if (!lookup.offsetSeconds.has_value()) {
            return TimeScaleConversionResult::failureFromLookup(
                epoch, targetScale, lookup.warningCodeMask, lookup.diagnosticText
            );
        }

        TimeScaleConversionResult result = TimeScaleConversionResult::success(
            epoch.addSeconds(targetOffsetSeconds(targetScale, static_cast<double>(*lookup.offsetSeconds)), targetScale)
        );
        applyLookup(result, lookup);
        return result;
    }
    case skygate::core::TimeScale::Tai:
        if (targetScale == skygate::core::TimeScale::Tdb) {
            return convertVia(skygate::core::TimeScale::Tt, epoch, targetScale);
        }
        if (targetScale == skygate::core::TimeScale::Tt) {
            return TimeScaleConversionResult::success(
                epoch.addSeconds(TimeConstants::kTtMinusTaiSeconds, skygate::core::TimeScale::Tt)
            );
        }
        if (targetScale == skygate::core::TimeScale::Utc) {
            const LeapSecondOffsetResolver::Result lookup = m_leapSecondOffsetResolver.lookupTaiOffset(epoch);
            if (!lookup.offsetSeconds.has_value()) {
                return TimeScaleConversionResult::failureFromLookup(
                    epoch, targetScale, lookup.warningCodeMask, lookup.diagnosticText
                );
            }

            TimeScaleConversionResult result = TimeScaleConversionResult::success(
                epoch.addSeconds(-static_cast<double>(*lookup.offsetSeconds), skygate::core::TimeScale::Utc)
            );
            applyLookup(result, lookup);
            return result;
        }
        if (targetScale == skygate::core::TimeScale::Ut1) {
            return convertVia(skygate::core::TimeScale::Utc, epoch, targetScale);
        }
        break;
    case skygate::core::TimeScale::Tt:
        if (targetScale == skygate::core::TimeScale::Ut1) {
            return convertVia(skygate::core::TimeScale::Utc, epoch, targetScale);
        }
        if (targetScale == skygate::core::TimeScale::Tdb) {
            return TdbTtConverter::ttToTdb(epoch);
        }
        if (targetScale == skygate::core::TimeScale::Tai) {
            return TimeScaleConversionResult::success(
                epoch.addSeconds(-TimeConstants::kTtMinusTaiSeconds, skygate::core::TimeScale::Tai)
            );
        }
        if (targetScale == skygate::core::TimeScale::Utc) {
            return convert(
                epoch.addSeconds(-TimeConstants::kTtMinusTaiSeconds, skygate::core::TimeScale::Tai),
                skygate::core::TimeScale::Utc
            );
        }
        break;
    case skygate::core::TimeScale::Tdb:
        if (targetScale == skygate::core::TimeScale::Tt) {
            return TdbTtConverter::tdbToTt(epoch);
        }
        return convertVia(skygate::core::TimeScale::Tt, epoch, targetScale);
    case skygate::core::TimeScale::Ut1:
        if (targetScale == skygate::core::TimeScale::Utc) {
            return m_ut1OffsetResolver.lookupUtcFromUt1(epoch);
        }
        return convertVia(skygate::core::TimeScale::Utc, epoch, targetScale);
    }

    TimeScaleConversionResult result =
        TimeScaleConversionResult::failure(epoch, targetScale, "The requested time-scale conversion is not supported.");
    result.addWarning(TimeScaleConversionWarningCode::Type::UnsupportedConversion);
    return result;
}

TimeScaleConversionResult LeapSecondTimeScaleService::convertCivilDateTime(
    const skygate::core::CivilDateTime& dateTime, const skygate::core::TimeScale targetScale
) const
{
    if (!skygate::core::CalendarTime::isValidCivilDateTime(dateTime)) {
        skygate::core::AstronomicalEpoch epoch;
        epoch.timeScale = targetScale;
        TimeScaleConversionResult result =
            TimeScaleConversionResult::failure(epoch, targetScale, "Civil date/time input is invalid.");
        result.addWarning(TimeScaleConversionWarningCode::Type::InvalidInput);
        return result;
    }

    std::optional<skygate::core::AstronomicalEpoch> epoch;
    std::optional<skygate::core::AstronomicalEpoch> precedingEpoch;
    const std::optional<skygate::core::CivilDateTime> precedingSecond =
        skygate::core::CalendarTime::isUtcLeapSecondLabel(dateTime)
            ? skygate::core::CalendarTime::leapSecondPredecessor(dateTime)
            : std::nullopt;
    if (precedingSecond.has_value()) {
        precedingEpoch = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*precedingSecond);
        if (precedingEpoch.has_value()) {
            epoch = precedingEpoch->addSeconds(1.0, skygate::core::TimeScale::Utc);
        }
    } else {
        epoch = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(dateTime);
    }

    if (!epoch.has_value()) {
        skygate::core::AstronomicalEpoch failedEpoch;
        failedEpoch.timeScale = targetScale;
        TimeScaleConversionResult result = TimeScaleConversionResult::failure(
            failedEpoch, targetScale, "Civil date/time input could not be converted to an epoch."
        );
        result.addWarning(TimeScaleConversionWarningCode::Type::InvalidInput);
        return result;
    }

    if (!precedingSecond.has_value()) {
        return convert(*epoch, targetScale);
    }

    if (!precedingEpoch.has_value()) {
        TimeScaleConversionResult result =
            TimeScaleConversionResult::failure(*epoch, targetScale, "Leap-second civil date/time input is invalid.");
        result.addWarning(TimeScaleConversionWarningCode::Type::InvalidInput);
        return result;
    }

    const LeapSecondOffsetResolver::Result precedingLookup =
        m_leapSecondOffsetResolver.lookupUtcOffset(*precedingEpoch);
    const LeapSecondOffsetResolver::Result nextLookup = m_leapSecondOffsetResolver.lookupUtcOffset(*epoch);
    if (!precedingLookup.offsetSeconds.has_value() || !nextLookup.offsetSeconds.has_value()
        || *nextLookup.offsetSeconds != *precedingLookup.offsetSeconds + 1) {
        TimeScaleConversionResult result = TimeScaleConversionResult::failureFromLookup(
            *epoch,
            targetScale,
            precedingLookup.warningCodeMask,
            "Civil date/time does not identify a configured positive leap second."
        );
        result.warningCodeMask.merge(nextLookup.warningCodeMask);
        result.addWarning(TimeScaleConversionWarningCode::Type::InvalidInput);
        return result;
    }

    if (targetScale == skygate::core::TimeScale::Utc) {
        return TimeScaleConversionResult::success(*epoch);
    }
    if (targetScale != skygate::core::TimeScale::Tai && targetScale != skygate::core::TimeScale::Tt
        && targetScale != skygate::core::TimeScale::Tdb) {
        TimeScaleConversionResult result = TimeScaleConversionResult::failure(
            *epoch, targetScale, "Only UTC leap-second conversion to TAI, TT, or TDB is currently supported."
        );
        result.addWarning(TimeScaleConversionWarningCode::Type::UnsupportedConversion);
        return result;
    }

    const double taiOffset = static_cast<double>(*precedingLookup.offsetSeconds);
    const double targetOffset = targetOffsetSeconds(targetScale, taiOffset);
    TimeScaleConversionResult result = TimeScaleConversionResult::success(epoch->addSeconds(
        targetOffset, targetScale == skygate::core::TimeScale::Tdb ? skygate::core::TimeScale::Tt : targetScale
    ));
    TimeScaleConversionResult::mergeWarnings(result, precedingLookup.status, precedingLookup.warningCodeMask);
    TimeScaleConversionResult::mergeWarnings(result, nextLookup.status, nextLookup.warningCodeMask);
    if (targetScale == skygate::core::TimeScale::Tdb) {
        TimeScaleConversionResult tdb = convert(result.epoch, skygate::core::TimeScale::Tdb);
        TimeScaleConversionResult::mergeWarnings(tdb, result.status, result.warningCodeMask);
        return tdb;
    }
    return result;
}

TimeScaleConversionResult LeapSecondTimeScaleService::convertVia(
    const skygate::core::TimeScale intermediateScale,
    const skygate::core::AstronomicalEpoch& epoch,
    const skygate::core::TimeScale targetScale
) const
{
    TimeScaleConversionResult intermediate = convert(epoch, intermediateScale);
    if (!intermediate.isSuccess()) {
        intermediate.epoch.timeScale = targetScale;
        return intermediate;
    }

    TimeScaleConversionResult result = convert(intermediate.epoch, targetScale);
    TimeScaleConversionResult::mergeWarnings(result, intermediate.status, intermediate.warningCodeMask);
    return result;
}

}  // namespace skygate::ephemeris
