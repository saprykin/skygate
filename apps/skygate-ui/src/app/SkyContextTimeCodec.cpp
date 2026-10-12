#include "SkyContextTimeCodec.hpp"
#include "time/SkyQtTimeCodec.hpp"

namespace skygate::ui::internal {

QDateTime SkyContextTimeCodec::toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime)
{
    return SkyQtTimeCodec::toQDateTimeUtc(utcTime);
}

skygate::core::UtcTimePoint SkyContextTimeCodec::toUtcTimePoint(const QDateTime& utcTime)
{
    return SkyQtTimeCodec::toUtcTimePoint(utcTime);
}

}  // namespace skygate::ui::internal
