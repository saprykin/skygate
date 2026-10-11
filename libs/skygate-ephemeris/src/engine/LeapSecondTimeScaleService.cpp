#include "LeapSecondTimeScaleService.hpp"
#include "TdbTtConverter.hpp"
#include "math/TimeConstants.hpp"
#include "time/CalendarTime.hpp"

#include <optional>

namespace skygate::ephemeris {

namespace {

using skygate::core::TimeConstants;

[[nodiscard]] bool isUtcLeapSecondLabel(const skygate::core::CivilDateTime& dateTime) noexcept
{
    return dateTime.timeScale == skygate::core::TimeScale::Utc && dateTime.hour == 23 && dateTime.minute == 59
           && dateTime.second == 60;
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
                TimeScaleConversionResult result =
                    TimeScaleConversionResult::failure(epoch, targetScale, lookup.diagnosticText);
                result.warningCodeMask = lookup.warningCodeMask;
                return result;
            }

            TimeScaleConversionResult result = TimeScaleConversionResult::success(
                epoch.addSeconds(*lookup.ut1MinusUtcSeconds, skygate::core::TimeScale::Ut1)
            );
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
            TimeScaleConversionResult::mergeWarnings(tdb, tt.status, tt.warningCodeMask);
            return tdb;
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
            TimeScaleConversionResult result =
                TimeScaleConversionResult::failure(epoch, targetScale, lookup.diagnosticText);
            result.warningCodeMask = lookup.warningCodeMask;
            return result;
        }

        const double taiOffset = static_cast<double>(*lookup.offsetSeconds);
        const double targetOffset =
            targetScale == skygate::core::TimeScale::Tt ? taiOffset + TimeConstants::kTtMinusTaiSeconds : taiOffset;
        TimeScaleConversionResult result = TimeScaleConversionResult::success(
            epoch.addSeconds(targetOffset, targetScale), lookup.status, lookup.warningCodeMask
        );
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
            TimeScaleConversionResult::mergeWarnings(tdb, tt.status, tt.warningCodeMask);
            return tdb;
        }
        if (targetScale == skygate::core::TimeScale::Tt) {
            return TimeScaleConversionResult::success(
                epoch.addSeconds(TimeConstants::kTtMinusTaiSeconds, skygate::core::TimeScale::Tt)
            );
        }
        if (targetScale == skygate::core::TimeScale::Utc) {
            const LeapSecondOffsetResolver::Result lookup = m_leapSecondOffsetResolver.lookupTaiOffset(epoch);
            if (!lookup.offsetSeconds.has_value()) {
                TimeScaleConversionResult result =
                    TimeScaleConversionResult::failure(epoch, targetScale, lookup.diagnosticText);
                result.warningCodeMask = lookup.warningCodeMask;
                return result;
            }

            TimeScaleConversionResult result = TimeScaleConversionResult::success(
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
            TimeScaleConversionResult::mergeWarnings(ut1, utc.status, utc.warningCodeMask);
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
            TimeScaleConversionResult::mergeWarnings(ut1, utc.status, utc.warningCodeMask);
            return ut1;
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
    case skygate::core::TimeScale::Tdb: {
        TimeScaleConversionResult result = TdbTtConverter::tdbToTt(epoch);
        if (targetScale == skygate::core::TimeScale::Tt) {
            return result;
        }

        TimeScaleConversionResult converted = convert(result.epoch, targetScale);
        TimeScaleConversionResult::mergeWarnings(converted, result.status, result.warningCodeMask);
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
        TimeScaleConversionResult::mergeWarnings(converted, utc.status, utc.warningCodeMask);
        return converted;
    }
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
        TimeScaleConversionResult result = TimeScaleConversionResult::failure(
            failedEpoch, targetScale, "Civil date/time input could not be converted to an epoch."
        );
        result.addWarning(TimeScaleConversionWarningCode::Type::InvalidInput);
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
            TimeScaleConversionResult::failure(*epoch, targetScale, "Leap-second civil date/time input is invalid.");
        result.addWarning(TimeScaleConversionWarningCode::Type::InvalidInput);
        return result;
    }

    const LeapSecondOffsetResolver::Result precedingLookup =
        m_leapSecondOffsetResolver.lookupUtcOffset(*precedingEpoch);
    const LeapSecondOffsetResolver::Result nextLookup = m_leapSecondOffsetResolver.lookupUtcOffset(*epoch);
    if (!precedingLookup.offsetSeconds.has_value() || !nextLookup.offsetSeconds.has_value()
        || *nextLookup.offsetSeconds != *precedingLookup.offsetSeconds + 1) {
        TimeScaleConversionResult result = TimeScaleConversionResult::failure(
            *epoch, targetScale, "Civil date/time does not identify a configured positive leap second."
        );
        result.warningCodeMask = precedingLookup.warningCodeMask | nextLookup.warningCodeMask;
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
    const double targetOffset =
        targetScale == skygate::core::TimeScale::Tai ? taiOffset : taiOffset + TimeConstants::kTtMinusTaiSeconds;
    TimeScaleConversionResult result = TimeScaleConversionResult::success(epoch->addSeconds(
        targetOffset, targetScale == skygate::core::TimeScale::Tdb ? skygate::core::TimeScale::Tt : targetScale
    ));
    LeapSecondOffsetResolver::mergeWarnings(result, precedingLookup);
    LeapSecondOffsetResolver::mergeWarnings(result, nextLookup);
    if (targetScale == skygate::core::TimeScale::Tdb) {
        TimeScaleConversionResult tdb = convert(result.epoch, skygate::core::TimeScale::Tdb);
        TimeScaleConversionResult::mergeWarnings(tdb, result.status, result.warningCodeMask);
        return tdb;
    }
    return result;
}

}  // namespace skygate::ephemeris
