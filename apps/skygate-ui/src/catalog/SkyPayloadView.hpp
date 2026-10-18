#pragma once

#include <QByteArray>

#include <cstddef>
#include <string_view>

namespace skygate::ui::internal {

// Read-only view over the bytes of a payload. The view shares the payload's
// storage, so it stays valid while that payload is alive and unchanged.
class SkyPayloadView final {
public:
    SkyPayloadView() = delete;

    [[nodiscard]] static std::string_view of(const QByteArray& payload) noexcept
    {
        return std::string_view(payload.constData(), static_cast<std::size_t>(payload.size()));
    }
};

}  // namespace skygate::ui::internal
