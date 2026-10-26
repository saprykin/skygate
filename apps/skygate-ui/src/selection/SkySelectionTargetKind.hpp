#pragma once

#include <QString>

#include <cstdint>
#include <optional>

namespace skygate::ui::internal {

// Token that tells the selection and search layers what a target id refers
// to. The token values travel to QML, so they are part of the UI contract.
class SkySelectionTargetKind final {
public:
    enum class Type : std::uint8_t {
        Body = 0,
        ConstellationLabel = 1
    };

    SkySelectionTargetKind() = delete;

    [[nodiscard]] static QString token(Type kind);
    // Reads the canonical token and the older all-lowercase spelling of the
    // constellation label token.
    [[nodiscard]] static std::optional<Type> fromToken(const QString& text);
    [[nodiscard]] static bool isBody(const QString& text);
    [[nodiscard]] static bool isConstellationLabel(const QString& text);
};

}  // namespace skygate::ui::internal
