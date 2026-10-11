#include "CalcephKernel.hpp"
#include "engine/EphemerisLogging.hpp"
#include "engine/ITimeScaleService.hpp"
#include "math/PhysicalConstants.hpp"

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
#include <calceph.h>
#endif

#include <QDebug>
#include <QString>

#include <array>
#include <optional>
#include <string>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
thread_local std::string g_lastCalcephError;

void captureCalcephError(const char* message)
{
    g_lastCalcephError = message != nullptr ? std::string{message} : std::string{};
}
#endif

// Kernel epochs are expressed in TDB while the manifest validity range is
// expressed in UTC. The conversion runs through the shared time-scale service
// so leap seconds come from the same table the rest of the engine uses.
[[nodiscard]] std::optional<skygate::core::AstronomicalEpoch> utcEpochForTdbEpoch(
    const skygate::core::AstronomicalEpoch& epoch, const skygate::ephemeris::ITimeScaleService* timeScaleService
)
{
    if (epoch.timeScale != skygate::core::TimeScale::Tdb || !epoch.isFinite() || timeScaleService == nullptr) {
        return std::nullopt;
    }

    const TimeScaleConversionResult conversion = timeScaleService->convert(epoch, skygate::core::TimeScale::Utc);
    if (!conversion.isSuccess()) {
        return std::nullopt;
    }

    return conversion.epoch;
}

}  // namespace

struct CalcephKernel::Impl final {
    explicit Impl(Info info, std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService)
        : m_kernelInfo(std::move(info)), m_timeScaleService(std::move(timeScaleService))
    {
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
        calceph_seterrorhandler(3, captureCalcephError);
        m_handle = calceph_open(m_kernelInfo->activePath.generic_string().c_str());
        if (m_handle == nullptr) {
            m_status = Status::OpenFailed;
            const std::string openError = g_lastCalcephError;
            g_lastCalcephError.clear();
            m_diagnostics.push_back(
                openError.empty() ? "CALCEPH failed to open the selected solar-system kernel."
                                  : "CALCEPH failed to open the selected solar-system kernel: " + openError
            );
            qCWarning(skygateEphemerisEngineLog).noquote() << QString::fromStdString(m_diagnostics.back());
            return;
        }

        m_status = Status::Ready;
#else
        m_status = Status::CalcephUnavailable;
        m_diagnostics.push_back("CALCEPH support is not enabled in this build.");
#endif
    }

    ~Impl()
    {
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
        if (m_handle != nullptr) {
            calceph_close(m_handle);
        }
#endif
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    [[nodiscard]] Status statusForEpoch(const skygate::core::AstronomicalEpoch& epoch) const noexcept
    {
        if (m_status != Status::Ready || !m_kernelInfo.has_value()) {
            return m_status;
        }
        if (!epoch.isFinite()) {
            return Status::OutOfRange;
        }

        if (epoch.timeScale == skygate::core::TimeScale::Tdb) {
            const std::optional<skygate::core::AstronomicalEpoch> utcEpoch =
                utcEpochForTdbEpoch(epoch, m_timeScaleService.get());
            if (utcEpoch.has_value()) {
                return m_kernelInfo->validityRange.contains(*utcEpoch) ? Status::Ready : Status::OutOfRange;
            }
        }

        return m_kernelInfo->validityRange.contains(epoch) ? Status::Ready : Status::OutOfRange;
    }

    [[nodiscard]] ICalcephKernel::StateResult
    compute(const skygate::core::AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId) const
    {
        ICalcephKernel::StateResult result;
        result.metadata.dataSourceProvenance = "CALCEPH solar-system kernel";

        if (epoch.timeScale != skygate::core::TimeScale::Tdb) {
            result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            result.metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
            return result;
        }

        const Status epochStatus = statusForEpoch(epoch);
        if (epochStatus != Status::Ready) {
            result.metadata.status = epochStatus == Status::OutOfRange ? EphemerisEngineQueryStatus::Type::OutOfRange
                                                                       : EphemerisEngineQueryStatus::Type::Failed;
            result.metadata.addWarning(
                epochStatus == Status::OutOfRange ? EphemerisEngineWarning::Code::DataOutOfRange
                                                  : EphemerisEngineWarning::Code::MissingEphemerisData
            );
            return result;
        }

        result.metadata.dataSourceProvenance = m_kernelInfo->provenance;
        result.metadata.effectiveDataValidityRange = m_kernelInfo->validityRange;

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
        std::array<double, 6> positionVelocity{};
        g_lastCalcephError.clear();
        const int calcephResult = calceph_compute_unit(
            m_handle,
            epoch.julianDatePart1,
            epoch.julianDatePart2,
            targetNaifId,
            centerNaifId,
            CALCEPH_USE_NAIFID + CALCEPH_UNIT_KM + CALCEPH_UNIT_DAY,
            positionVelocity.data()
        );
        if (calcephResult == 0) {
            const std::string computeError = g_lastCalcephError;
            result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
            if (!computeError.empty()) {
                result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed, computeError);
                m_diagnostics.push_back(computeError);
            }
            qCWarning(skygateEphemerisEngineLog).noquote()
                << "CALCEPH compute failed for target" << targetNaifId << "center" << centerNaifId
                << (computeError.empty() ? QStringLiteral("unknown CALCEPH error")
                                         : QString::fromStdString(computeError));
            return result;
        }

        result.positionAu = skygate::core::Vector3d{
            .x = positionVelocity[0] / skygate::core::PhysicalConstants::kAstronomicalUnitKilometers,
            .y = positionVelocity[1] / skygate::core::PhysicalConstants::kAstronomicalUnitKilometers,
            .z = positionVelocity[2] / skygate::core::PhysicalConstants::kAstronomicalUnitKilometers,
        };
        result.velocityAuPerDay = skygate::core::Vector3d{
            .x = positionVelocity[3] / skygate::core::PhysicalConstants::kAstronomicalUnitKilometers,
            .y = positionVelocity[4] / skygate::core::PhysicalConstants::kAstronomicalUnitKilometers,
            .z = positionVelocity[5] / skygate::core::PhysicalConstants::kAstronomicalUnitKilometers,
        };
        result.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
#else
        result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
        result.metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
#endif

        return result;
    }

    Status m_status = Status::OpenFailed;
    mutable std::vector<std::string> m_diagnostics;
    std::optional<Info> m_kernelInfo;
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> m_timeScaleService;
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    t_calcephbin* m_handle = nullptr;
#endif
};

CalcephKernel::CalcephKernel(Info info, std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService)
    : m_impl(std::make_unique<Impl>(std::move(info), std::move(timeScaleService)))
{
}

CalcephKernel::~CalcephKernel() = default;

ICalcephKernel::Status CalcephKernel::status() const noexcept
{
    return m_impl->m_status;
}

const std::vector<std::string>& CalcephKernel::diagnostics() const noexcept
{
    return m_impl->m_diagnostics;
}

const std::optional<ICalcephKernel::Info>& CalcephKernel::kernelInfo() const noexcept
{
    return m_impl->m_kernelInfo;
}

ICalcephKernel::Status CalcephKernel::statusForEpoch(const skygate::core::AstronomicalEpoch& epoch) const noexcept
{
    return m_impl->statusForEpoch(epoch);
}

ICalcephKernel::StateResult CalcephKernel::compute(
    const skygate::core::AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId
) const
{
    return m_impl->compute(epoch, targetNaifId, centerNaifId);
}

}  // namespace skygate::ephemeris::highprecision
