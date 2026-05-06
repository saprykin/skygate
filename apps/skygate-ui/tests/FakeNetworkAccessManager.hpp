#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
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
            finish();
        });
    }

    void abort() override
    {
        if (isFinished()) {
            return;
        }

        m_aborted = true;
        m_payload.clear();
        m_offset = 0;
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 0);
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("Operation canceled"));
        finish();
    }

    [[nodiscard]] bool isAborted() const noexcept
    {
        return m_aborted;
    }

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
    void finish()
    {
        if (isFinished()) {
            return;
        }

        setFinished(true);
        if (error() != QNetworkReply::NoError && !m_errorSignalEmitted) {
            m_errorSignalEmitted = true;
            emit errorOccurred(error());
        }
        if (!m_payload.isEmpty()) {
            emit readyRead();
        }
        emit finished();
    }

    QByteArray m_payload;
    qsizetype m_offset = 0;
    bool m_aborted = false;
    bool m_errorSignalEmitted = false;
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

    [[nodiscard]] QStringList finishedUrls() const
    {
        return m_finishedUrls;
    }

    [[nodiscard]] QStringList abortedUrls() const
    {
        return m_abortedUrls;
    }

    [[nodiscard]] QList<FakeNetworkReply*> issuedReplies() const
    {
        QList<FakeNetworkReply*> replies;
        replies.reserve(m_issuedReplies.size());
        for (const QPointer<FakeNetworkReply>& reply : m_issuedReplies) {
            if (!reply.isNull()) {
                replies.push_back(reply.data());
            }
        }
        return replies;
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
        auto* reply = new FakeNetworkReply(request, std::move(response), this);
        m_issuedReplies.push_back(reply);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply] {
            const QString url = reply->url().toString();
            m_finishedUrls.push_back(url);
            if (reply->isAborted()) {
                m_abortedUrls.push_back(url);
            }
        });
        return reply;
    }

private:
    QHash<QString, QQueue<FakeNetworkResponse>> m_responses;
    QStringList m_requestedUrls;
    QStringList m_finishedUrls;
    QStringList m_abortedUrls;
    QList<QPointer<FakeNetworkReply>> m_issuedReplies;
};

}  // namespace skygate::ui::tests
