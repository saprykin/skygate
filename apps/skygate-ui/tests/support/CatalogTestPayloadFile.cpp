#include "CatalogTestPayloadFile.hpp"

#include <QFile>

namespace skygate::ui::tests {

bool CatalogTestPayloadFile::write(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

}  // namespace skygate::ui::tests
