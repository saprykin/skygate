#pragma once

#include <QSize>

namespace skygate::ui::internal {

// Durable main window geometry. A stored size below the minimum is replaced
// by the default so an unusable geometry never becomes the next start's
// window size.
class SkyWindowSizeSettings final {
public:
    static constexpr int kDefaultMainWindowWidth = 1100;
    static constexpr int kDefaultMainWindowHeight = 760;
    static constexpr int kMinimumMainWindowWidth = 560;
    static constexpr int kMinimumMainWindowHeight = 420;

    [[nodiscard]] static bool save(const QSize& size);
    [[nodiscard]] static QSize load();
};

}  // namespace skygate::ui::internal
