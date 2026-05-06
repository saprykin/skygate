#pragma once

#include <QByteArray>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <utility>

namespace skygate::ui::tests {

struct FakeNetworkResponse final {
    QByteArray payload;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    QString errorText;
    int httpStatusCode = 200;
    int delayMs = 0;
};

class FakeNetworkReply final : public QNetworkReply {
public:
    FakeNetworkReply(const QNetworkRequest& request, FakeNetworkResponse response, QObject* parent)
        : QNetworkReply(parent)
        , m_payload(std::move(response.payload))
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.httpStatusCode);
        setHeader(QNetworkRequest::ContentLengthHeader, m_payload.size());
        if (response.error != QNetworkReply::NoError) {
            setError(response.error, response.errorText);
        }
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);

        QTimer::singleShot(response.delayMs, this, [this] {
            if (!m_payload.isEmpty()) {
                emit readyRead();
            }
            emit finished();
        });
    }

    void abort() override {}

    [[nodiscard]] qint64 bytesAvailable() const override
    {
        return static_cast<qint64>(m_payload.size() - m_offset) + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char* data, const qint64 maxSize) override
    {
        if (m_offset >= m_payload.size()) {
            return -1;
        }

        const qint64 bytesToRead = std::min<qint64>(
            maxSize,
            static_cast<qint64>(m_payload.size() - m_offset)
        );
        std::memcpy(data, m_payload.constData() + m_offset, static_cast<std::size_t>(bytesToRead));
        m_offset += bytesToRead;
        return bytesToRead;
    }

private:
    QByteArray m_payload;
    qsizetype m_offset = 0;
};

class FakeNetworkAccessManager final : public QNetworkAccessManager {
public:
    void enqueueResponse(const QString& url, FakeNetworkResponse response)
    {
        m_responses[url].enqueue(std::move(response));
    }

    [[nodiscard]] QStringList requestedUrls() const
    {
        return m_requestedUrls;
    }

protected:
    QNetworkReply* createRequest(
        const Operation operation,
        const QNetworkRequest& request,
        QIODevice* outgoingData = nullptr
    ) override
    {
        (void)operation;
        (void)outgoingData;

        const QString url = request.url().toString();
        m_requestedUrls.push_back(url);
        FakeNetworkResponse response;
        auto responseIt = m_responses.find(url);
        if (responseIt != m_responses.end() && !responseIt->isEmpty()) {
            response = responseIt->dequeue();
        } else {
            response.error = QNetworkReply::HostNotFoundError;
            response.errorText = QStringLiteral("No fake response registered");
            response.httpStatusCode = 404;
        }
        return new FakeNetworkReply(request, std::move(response), this);
    }

private:
    QHash<QString, QQueue<FakeNetworkResponse>> m_responses;
    QStringList m_requestedUrls;
};

}  // namespace skygate::ui::tests
