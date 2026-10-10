#pragma once

#include <QByteArray>
#include <QString>

namespace skygate::ui::tests {

// Writes payload bytes to a test fixture path so a scenario can address them
// as a local file URL.
class CatalogTestPayloadFile final {
public:
    [[nodiscard]] static bool write(const QString& path, const QByteArray& contents);
};

}  // namespace skygate::ui::tests
