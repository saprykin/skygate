#pragma once

#include "ICalcephKernelProvider.hpp"

#include <memory>
#include <string>

namespace skygate::ephemeris {
class IEphemerisDataSnapshot;
class EphemerisDataManifest;
class ITimeScaleService;
}  // namespace skygate::ephemeris

namespace skygate::ephemeris::highprecision {

class CalcephKernelProvider final : public ICalcephKernelProvider {
public:
    struct Options {
        std::string preferredProfileId;
        bool preferLongRange = false;
        bool verifyChecksum = true;
    };

    CalcephKernelProvider(const IEphemerisDataSnapshot& snapshot, const EphemerisDataManifest& manifest);
    CalcephKernelProvider(
        const IEphemerisDataSnapshot& snapshot, const EphemerisDataManifest& manifest, Options options
    );
    CalcephKernelProvider(
        const IEphemerisDataSnapshot& snapshot,
        const EphemerisDataManifest& manifest,
        Options options,
        std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService
    );
    ~CalcephKernelProvider() override;

    CalcephKernelProvider(const CalcephKernelProvider&) = delete;
    CalcephKernelProvider& operator=(const CalcephKernelProvider&) = delete;
    CalcephKernelProvider(CalcephKernelProvider&&) noexcept = delete;
    CalcephKernelProvider& operator=(CalcephKernelProvider&&) noexcept = delete;

    [[nodiscard]] std::shared_ptr<const ICalcephKernel> openKernel() const override;

private:
    class Impl;

    std::unique_ptr<Impl> m_impl;
};

}  // namespace skygate::ephemeris::highprecision
