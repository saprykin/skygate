#include "SkyQtTimeCodec.hpp"

#include "skygate/core/UtcTimeCodec.hpp"

#include <QTimeZone>

namespace {

qint64 floorDiv(const qint64 numerator, const qint64 denominator) noexcept
{
    qint64 quotient = numerator / denominator;
    const qint64 remainder = numerator % denominator;
    if (remainder != 0 && ((remainder < 0) != (denominator < 0))) {
        --quotient;
    }
    return quotient;
}

}  // namespace

QDateTime SkyQtTimeCodec::toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime)
{
    const qint64 epochMicros = static_cast<qint64>(skygate::core::UtcTimeCodec::toEpochMicros(utcTime));
    return QDateTime::fromMSecsSinceEpoch(floorDiv(epochMicros, 1000), QTimeZone::UTC);
}

skygate::core::UtcTimePoint SkyQtTimeCodec::toUtcTimePoint(const QDateTime& utcDateTime)
{
    return skygate::core::UtcTimeCodec::fromEpochMicros(
        static_cast<std::int64_t>(utcDateTime.toUTC().toMSecsSinceEpoch()) * 1000
    );
}
