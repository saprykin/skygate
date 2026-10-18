#pragma once

#include <string>
#include <string_view>
#include <type_traits>

namespace skygate::ephemeris {

// Diagnostics shared by the result types that report a payload read, a staged
// verification, or an activation.
class EphemerisPayloadDiagnostics final {
public:
    EphemerisPayloadDiagnostics() = delete;

    template <typename Result>
    static void addDiagnostic(Result& result, const std::string_view diagnostic)
    {
        result.diagnostics.emplace_back(diagnostic);
    }

    template <typename Result>
    static void markCanceled(Result& result, const std::string_view diagnostic)
    {
        using CancelStatus = std::remove_cvref_t<decltype(result.status)>;
        result.status = CancelStatus::Canceled;
        addDiagnostic(result, diagnostic);
    }
};

}  // namespace skygate::ephemeris
