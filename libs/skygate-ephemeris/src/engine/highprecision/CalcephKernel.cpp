#include "CalcephKernel.hpp"

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
#include "math/PhysicalConstants.hpp"
#include <calceph.h>
#include <array>
#endif

#include <utility>

namespace skygate::ephemeris::highprecision {

struct CalcephKernel::Impl final {
    explicit Impl(Info info) : m_kernelInfo(std::move(info))
    {
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
        m_handle = calceph_open(m_kernelInfo->activePath.generic_string().c_str());
        if (m_handle == nullptr) {
            m_status = Status::OpenFailed;
            m_diagnostics.push_back("CALCEPH failed to open the selected solar-system kernel.");
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

    [[nodiscard]] Status statusForEpoch(const AstronomicalEpoch& epoch) const noexcept
    {
        if (m_status != Status::Ready || !m_kernelInfo.has_value()) {
            return m_status;
        }
        if (!epoch.isFinite() || epoch.sortKey() < m_kernelInfo->validityRange.start.sortKey()
            || epoch.sortKey() > m_kernelInfo->validityRange.end.sortKey()) {
            return Status::OutOfRange;
        }

        return Status::Ready;
    }

    [[nodiscard]] SolarSystemKernelStateResult
    compute(const AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId) const
    {
        SolarSystemKernelStateResult result;
        result.metadata.dataSourceProvenance = "CALCEPH solar-system kernel";

        if (epoch.timeScale != TimeScale::Tdb) {
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
            result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
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
    std::vector<std::string> m_diagnostics;
    std::optional<Info> m_kernelInfo;
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    t_calcephbin* m_handle = nullptr;
#endif
};

CalcephKernel::CalcephKernel(Info info) : m_impl(std::make_unique<Impl>(std::move(info))) {}

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

ICalcephKernel::Status CalcephKernel::statusForEpoch(const AstronomicalEpoch& epoch) const noexcept
{
    return m_impl->statusForEpoch(epoch);
}

SolarSystemKernelStateResult
CalcephKernel::compute(const AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId) const
{
    return m_impl->compute(epoch, targetNaifId, centerNaifId);
}

}  // namespace skygate::ephemeris::highprecision
