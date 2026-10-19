#include "SkySceneDegradationReporter.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EphemerisDateRange.hpp"
#include "time/SkyQtTimeCodec.hpp"

#include <QString>
#include <QStringList>

#include <optional>
#include <utility>

namespace {

void appendUniqueReason(QStringList& reasons, const QString& reason)
{
    if (!reasons.contains(reason)) {
        reasons.push_back(reason);
    }
}

[[nodiscard]] bool isKernelRange(const skygate::ephemeris::EphemerisDateRange& range)
{
    const QString id = QString::fromStdString(range.id).toLower();
    const QString displayName = QString::fromStdString(range.displayName).toLower();
    return id.contains(QStringLiteral("kernel")) || displayName.contains(QStringLiteral("kernel"));
}

void appendRangeWarning(
    QStringList& reasons,
    const skygate::core::AstronomicalEpoch& epoch,
    const std::optional<skygate::ephemeris::EphemerisDateRange>& range,
    const QString& label
)
{
    if (!range.has_value() || range->contains(epoch)) {
        return;
    }

    appendUniqueReason(
        reasons,
        QStringLiteral("%1 out of range. Supported range: %2.").arg(label, SkyQtTimeCodec::formatDateRangeText(*range))
    );
}

[[nodiscard]] QVariantList
buildReasons(const SkyEphemerisSession::RequestContext& context, const skygate::ephemeris::IEphemerisEngine* engine)
{
    QStringList reasons;
    if (engine != nullptr) {
        for (const skygate::ephemeris::EphemerisDateRange& range : engine->supportedDateRanges()) {
            if (isKernelRange(range) && !range.contains(context.request.epoch)) {
                appendUniqueReason(
                    reasons,
                    QStringLiteral("Planetary kernel out of range. Supported range: %1.")
                        .arg(SkyQtTimeCodec::formatDateRangeText(range))
                );
            }
        }
    }

    appendRangeWarning(
        reasons, context.request.epoch, context.earthOrientationDataRange, QStringLiteral("Earth orientation data")
    );
    appendRangeWarning(
        reasons, context.request.epoch, context.leapSecondTableRange, QStringLiteral("Leap-second table")
    );
    appendRangeWarning(reasons, context.request.epoch, context.deltaTDataRange, QStringLiteral("Delta T data"));

    QVariantList result;
    result.reserve(reasons.size());
    for (const QString& reason : reasons) {
        result.push_back(reason);
    }
    return result;
}

}  // namespace

QVariantList SkySceneDegradationReporter::reasons() const
{
    return m_reasons;
}

bool SkySceneDegradationReporter::hasReasons() const
{
    return !m_reasons.isEmpty();
}

bool SkySceneDegradationReporter::update(
    const SkyEphemerisSession::RequestContext& context, const skygate::ephemeris::IEphemerisEngine* engine
)
{
    QVariantList reasons = buildReasons(context, engine);
    if (m_reasons == reasons) {
        return false;
    }

    m_reasons = std::move(reasons);
    return true;
}

void SkySceneDegradationReporter::clear()
{
    m_reasons = {};
}
