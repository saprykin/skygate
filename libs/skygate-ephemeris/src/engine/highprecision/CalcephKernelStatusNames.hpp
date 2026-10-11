#pragma once

#include "ICalcephKernel.hpp"

#include <string_view>

namespace skygate::ephemeris::highprecision {

class CalcephKernelStatusNames final {
public:
    CalcephKernelStatusNames() = delete;

    [[nodiscard]] static constexpr std::string_view displayName(const ICalcephKernel::Status status) noexcept
    {
        switch (status) {
        case ICalcephKernel::Status::Ready:
            return "ready";
        case ICalcephKernel::Status::CalcephUnavailable:
            return "calceph-unavailable";
        case ICalcephKernel::Status::MissingManifestProfile:
            return "missing-manifest-profile";
        case ICalcephKernel::Status::MissingKernelAsset:
            return "missing-kernel-asset";
        case ICalcephKernel::Status::InvalidKernelAsset:
            return "invalid-kernel-asset";
        case ICalcephKernel::Status::MissingKernelFile:
            return "missing-kernel-file";
        case ICalcephKernel::Status::ChecksumMismatch:
            return "checksum-mismatch";
        case ICalcephKernel::Status::OpenFailed:
            return "open-failed";
        case ICalcephKernel::Status::OutOfRange:
            return "out-of-range";
        }

        return {};
    }
};

}  // namespace skygate::ephemeris::highprecision
