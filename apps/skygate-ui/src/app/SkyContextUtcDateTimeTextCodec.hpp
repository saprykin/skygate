#pragma once

#include <QDateTime>
#include <QString>

namespace skygate::ui::internal {

class SkyContextUtcDateTimeTextCodec final {
public:
    struct ParseResult final {
        QDateTime utcDateTime;
        QString errorText;

        [[nodiscard]] bool isValid() const noexcept;
    };

    [[nodiscard]] static ParseResult parse(const QString& utcDateText, const QString& utcTimeText);
    [[nodiscard]] static QString formatDate(const QDateTime& utcTime);
};

}  // namespace skygate::ui::internal
