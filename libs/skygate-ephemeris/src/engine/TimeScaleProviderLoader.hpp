#pragma once

#include "IEarthOrientationProvider.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "ITimeScaleService.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace skygate::ephemeris {

class EphemerisDataManifest;

namespace highprecision {
class ICalcephKernelProvider;
}  // namespace highprecision

/// Composes the providers that the ephemeris engine bootstrap needs from an
/// ephemeris data snapshot. The engine factory and the backend resource cache
/// resolve providers here so the chain has one owner that reports what could
/// not be composed.
class TimeScaleProviderLoader final {
public:
    enum class Status : std::uint8_t {
        Loaded,
        MissingData,
        UnusableData
    };

    template <typename Provider>
    struct Result {
        std::shared_ptr<const Provider> provider;
        Status status = Status::MissingData;
        std::string diagnosticText;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return provider != nullptr;
        }
    };

    /// Provider set that `resolveMissing()` composes. Providers the caller
    /// already holds are kept as they are.
    struct Providers {
        std::shared_ptr<const ITimeScaleService> timeScaleService;
        std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider;
        std::shared_ptr<const highprecision::ICalcephKernelProvider> calcephKernelProvider;
    };

    /// Checksum policy for the CALCEPH kernel provider that `resolveMissing()`
    /// composes. Verifying re-reads the kernel file, so callers that reuse one
    /// provider across engine rebuilds disable it and rely on activation-time
    /// verification instead. `resolveMissing()` takes the policy as a required
    /// argument so no call site inherits another one's choice by accident.
    struct KernelOptions {
        bool verifyChecksum = true;
    };

    [[nodiscard]] static Result<IEarthOrientationProvider>
    loadEarthOrientationProvider(const IEphemerisDataSnapshot& snapshot);

    [[nodiscard]] static Result<ITimeScaleService> loadTimeScaleService(
        const IEphemerisDataSnapshot& snapshot,
        const std::shared_ptr<const IEarthOrientationProvider>& earthOrientationProvider
    );

    /// Composes every provider that `existing` does not already supply. A
    /// loader that rejects the snapshot data leaves its provider null and logs
    /// its diagnostic once. The kernel provider is composed only when a data
    /// manifest is available, because selecting a kernel needs one.
    [[nodiscard]] static Providers resolveMissing(
        const IEphemerisDataSnapshot& snapshot,
        const EphemerisDataManifest* dataManifest,
        const Providers& existing,
        const KernelOptions& kernelOptions
    );
};

}  // namespace skygate::ephemeris
