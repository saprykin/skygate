#pragma once

#include <QString>

#ifndef SKYGATE_QML_SOURCE_DIR
#define SKYGATE_QML_SOURCE_DIR ""
#endif

namespace skygate::ui::tests {

inline QString qmlSourcePath(const QString& fileName)
{
    return QStringLiteral(SKYGATE_QML_SOURCE_DIR) + QStringLiteral("/") + fileName;
}

}  // namespace skygate::ui::tests
