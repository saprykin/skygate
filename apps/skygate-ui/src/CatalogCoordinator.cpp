#include "CatalogCoordinator.hpp"

#include <QBuffer>
#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextStream>
#include <QUrl>
#include <QLocale>

#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <functional>
#include <optional>
#include <memory>
#include <queue>
#include <thread>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <zlib.h>

namespace {
constexpr std::size_t kMaxDownloadedCatalogBytes = 128U << 20;
constexpr std::size_t kMaxGunzipOutputBytes = 512U << 20;

class InMemoryStarCatalog final : public skygate::ephemeris::IStarCatalog {
public:
    explicit InMemoryStarCatalog(std::vector<skygate::ephemeris::CelestialBody> bodies)
        : m_bodies(std::move(bodies))
    {
    }

    [[nodiscard]] std::vector<skygate::ephemeris::CelestialBody> bodies() const override
    {
        return m_bodies;
    }

private:
    std::vector<skygate::ephemeris::CelestialBody> m_bodies;
};

std::string toLowerId(const std::string& value)
{
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return lower;
}

bool isSunOrMoonType(const skygate::ephemeris::CelestialBodyType type)
{
    return type == skygate::ephemeris::CelestialBodyType::Sun
        || type == skygate::ephemeris::CelestialBodyType::Moon;
}

bool isReferenceLineStarId(const std::string& id)
{
    constexpr std::array<std::string_view, 8> kReferenceLineStarIds = {{
        "sirius",
        "canopus",
        "arcturus",
        "vega",
        "capella",
        "rigel",
        "procyon",
        "betelgeuse",
    }};

    const std::string loweredId = toLowerId(id);
    return std::any_of(kReferenceLineStarIds.begin(), kReferenceLineStarIds.end(), [&loweredId](const auto value) {
        return loweredId == value;
    });
}

bool containsBodyIdCaseInsensitive(
    const std::vector<skygate::ephemeris::CelestialBody>& bodies,
    const std::string& id
)
{
    const std::string loweredId = toLowerId(id);
    return std::any_of(bodies.begin(), bodies.end(), [&loweredId](const auto& body) {
        return toLowerId(body.id) == loweredId;
    });
}

struct BrightnessSlotEntry {
    double visualMagnitude = 0.0;
    std::size_t slotIndex = 0;
    std::uint64_t generation = 0;
};

struct DimmestFirstCompare {
    [[nodiscard]] bool operator()(
        const BrightnessSlotEntry& lhs,
        const BrightnessSlotEntry& rhs
    ) const noexcept
    {
        if (lhs.visualMagnitude == rhs.visualMagnitude) {
            return lhs.slotIndex < rhs.slotIndex;
        }
        return lhs.visualMagnitude < rhs.visualMagnitude;
    }
};

constexpr std::size_t kMaxHygBodyCount = 25000;
constexpr std::size_t kHygProgressCallbackInterval = 512;

void parseCsvColumnsQt(const QString& line, QStringList& columns)
{
    columns.clear();

    QString currentColumn;
    currentColumn.reserve(line.size());
    bool insideQuotes = false;

    for (qsizetype index = 0; index < line.size(); ++index) {
        const QChar character = line.at(index);
        if (character == QLatin1Char('"')) {
            if (insideQuotes && index + 1 < line.size() && line.at(index + 1) == QLatin1Char('"')) {
                currentColumn.append(QLatin1Char('"'));
                ++index;
                continue;
            }
            insideQuotes = !insideQuotes;
            continue;
        }

        if (character == QLatin1Char(',') && !insideQuotes) {
            columns.push_back(currentColumn);
            currentColumn.clear();
            continue;
        }

        currentColumn.append(character);
    }

    columns.push_back(currentColumn);
}

[[nodiscard]] std::unique_ptr<skygate::ephemeris::IStarCatalog> parseHygCsvWithQt(
    const QByteArray& payload,
    const skygate::ephemeris::HygParseProgressCallback& progressCallback
)
{
    QBuffer buffer;
    buffer.setData(payload);
    if (!buffer.open(QIODevice::ReadOnly)) {
        return nullptr;
    }

    QTextStream textStream(&buffer);

    if (textStream.atEnd()) {
        return nullptr;
    }

    QStringList columns;
    parseCsvColumnsQt(textStream.readLine(), columns);

    QHash<QString, qsizetype> headerIndex;
    headerIndex.reserve(columns.size());
    for (qsizetype index = 0; index < columns.size(); ++index) {
        const QString header = columns[index].trimmed().toLower();
        if (!header.isEmpty()) {
            headerIndex.insert(header, index);
        }
    }

    const qsizetype raColumn = headerIndex.value("ra", -1);
    const qsizetype decColumn = headerIndex.value("dec", -1);
    const qsizetype magColumn = headerIndex.value("mag", -1);
    if (raColumn < 0 || decColumn < 0 || magColumn < 0) {
        return nullptr;
    }

    const qsizetype idColumn = headerIndex.value("id", -1);
    const qsizetype hipColumn = headerIndex.value("hip", -1);
    const qsizetype properColumn = headerIndex.value("proper", -1);
    const qsizetype bayerFlamsteedColumn = headerIndex.value("bf", -1);

    std::vector<skygate::ephemeris::CelestialBody> bodies;
    bodies.reserve(kMaxHygBodyCount);
    std::vector<std::uint64_t> slotGenerations;
    slotGenerations.reserve(kMaxHygBodyCount);
    std::priority_queue<
        BrightnessSlotEntry,
        std::vector<BrightnessSlotEntry>,
        DimmestFirstCompare
    > dimmestSelectedStars;

    const auto discardStaleHeapEntries = [&]() {
        while (!dimmestSelectedStars.empty()) {
            const BrightnessSlotEntry& topEntry = dimmestSelectedStars.top();
            if (topEntry.slotIndex >= bodies.size()) {
                dimmestSelectedStars.pop();
                continue;
            }
            if (slotGenerations[topEntry.slotIndex] != topEntry.generation) {
                dimmestSelectedStars.pop();
                continue;
            }
            break;
        }
    };

    std::size_t autoGeneratedIdCounter = 1;
    std::size_t parsedObjectCount = 0;
    const QLocale cLocale = QLocale::c();

    while (!textStream.atEnd()) {
        const QString line = textStream.readLine();
        if (line.trimmed().isEmpty()) {
            continue;
        }

        parseCsvColumnsQt(line, columns);

        const auto columnText = [&](const qsizetype columnIndex) -> QString {
            if (columnIndex < 0 || columnIndex >= columns.size()) {
                return {};
            }
            return columns[columnIndex].trimmed();
        };

        bool raOk = false;
        bool decOk = false;
        bool magOk = false;
        const double rightAscensionHours = cLocale.toDouble(columnText(raColumn), &raOk);
        const double declinationDeg = cLocale.toDouble(columnText(decColumn), &decOk);
        const double visualMagnitude = cLocale.toDouble(columnText(magColumn), &magOk);
        if (!raOk || !decOk || !magOk) {
            continue;
        }

        ++parsedObjectCount;
        if (progressCallback && (parsedObjectCount % kHygProgressCallbackInterval) == 0U) {
            progressCallback(parsedObjectCount);
        }

        skygate::ephemeris::CelestialBody body;
        body.type = skygate::ephemeris::CelestialBodyType::Star;
        body.visualMagnitude = visualMagnitude;
        body.fixedEquatorial = skygate::core::EquatorialCoordinate {
            .rightAscensionHours = rightAscensionHours,
            .declinationDeg = declinationDeg
        };

        const QString hipText = columnText(hipColumn);
        const QString idText = columnText(idColumn);
        if (!hipText.isEmpty()) {
            body.id = QString("hip_%1").arg(hipText).toStdString();
        } else if (!idText.isEmpty()) {
            body.id = QString("hyg_%1").arg(idText).toStdString();
        } else {
            body.id = "hyg_auto_" + std::to_string(autoGeneratedIdCounter++);
        }

        const QString properName = columnText(properColumn);
        const QString bayerFlamsteedName = columnText(bayerFlamsteedColumn);
        if (!properName.isEmpty()) {
            body.displayName = properName.toStdString();
        } else if (!bayerFlamsteedName.isEmpty()) {
            body.displayName = bayerFlamsteedName.toStdString();
        } else if (!hipText.isEmpty()) {
            body.displayName = QString("HIP %1").arg(hipText).toStdString();
        } else {
            body.displayName = body.id;
        }

        if (bodies.size() < kMaxHygBodyCount) {
            const std::size_t slotIndex = bodies.size();
            bodies.push_back(std::move(body));
            slotGenerations.push_back(0);
            dimmestSelectedStars.push(
                BrightnessSlotEntry {
                    .visualMagnitude = bodies.back().visualMagnitude,
                    .slotIndex = slotIndex,
                    .generation = slotGenerations[slotIndex]
                }
            );
        } else {
            discardStaleHeapEntries();
            if (dimmestSelectedStars.empty()) {
                continue;
            }

            const BrightnessSlotEntry dimmestSelected = dimmestSelectedStars.top();
            if (body.visualMagnitude >= dimmestSelected.visualMagnitude) {
                continue;
            }

            dimmestSelectedStars.pop();
            bodies[dimmestSelected.slotIndex] = std::move(body);
            ++slotGenerations[dimmestSelected.slotIndex];
            dimmestSelectedStars.push(
                BrightnessSlotEntry {
                    .visualMagnitude = bodies[dimmestSelected.slotIndex].visualMagnitude,
                    .slotIndex = dimmestSelected.slotIndex,
                    .generation = slotGenerations[dimmestSelected.slotIndex]
                }
            );
        }
    }

    if (progressCallback) {
        progressCallback(parsedObjectCount);
    }

    if (bodies.empty()) {
        return nullptr;
    }

    return std::make_unique<InMemoryStarCatalog>(std::move(bodies));
}

enum class CatalogPayloadFormat {
    PipeRows,
    HygCsv,
    HygCsvGzip,
    Unknown
};

std::optional<QByteArray> gunzipPayload(const QByteArray& payload)
{
    if (payload.isEmpty()) {
        return std::nullopt;
    }

    z_stream stream {};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(payload.constData()));
    stream.avail_in = static_cast<uInt>(payload.size());

    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        return std::nullopt;
    }

    QByteArray output;
    output.reserve(payload.size() * 3);

    std::array<char, 1 << 14> buffer {};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) {
            inflateEnd(&stream);
            return std::nullopt;
        }

        const std::size_t produced = buffer.size() - stream.avail_out;
        output.append(buffer.data(), static_cast<qsizetype>(produced));
        if (static_cast<std::size_t>(output.size()) > kMaxGunzipOutputBytes) {
            inflateEnd(&stream);
            return std::nullopt;
        }
    }

    inflateEnd(&stream);
    if (status != Z_STREAM_END || output.isEmpty()) {
        return std::nullopt;
    }

    return output;
}

std::string_view trimAsciiWhitespace(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

std::string_view firstNonEmptyLine(std::string_view data)
{
    std::size_t cursor = 0;
    while (cursor < data.size()) {
        const std::size_t newline = data.find('\n', cursor);
        const std::size_t lineEnd = newline == std::string_view::npos ? data.size() : newline;
        std::string_view line = trimAsciiWhitespace(data.substr(cursor, lineEnd - cursor));
        if (!line.empty() && line.front() != '#') {
            return line;
        }
        if (newline == std::string_view::npos) {
            break;
        }
        cursor = newline + 1;
    }
    return {};
}

bool containsCaseInsensitiveToken(const std::string_view value, const std::string_view token)
{
    const std::string loweredValue = toLowerId(std::string(value));
    const std::string loweredToken = toLowerId(std::string(token));
    return loweredValue.find(loweredToken) != std::string::npos;
}

CatalogPayloadFormat detectCatalogPayloadFormat(const QByteArray& payload)
{
    if (payload.size() >= 2) {
        const auto firstByte = static_cast<unsigned char>(payload.at(0));
        const auto secondByte = static_cast<unsigned char>(payload.at(1));
        if (firstByte == 0x1fU && secondByte == 0x8bU) {
            return CatalogPayloadFormat::HygCsvGzip;
        }
    }

    const std::string_view rows(payload.constData(), static_cast<std::size_t>(payload.size()));
    const std::string_view headerLine = firstNonEmptyLine(rows);
    if (headerLine.empty()) {
        return CatalogPayloadFormat::Unknown;
    }

    if (headerLine.find('|') != std::string_view::npos) {
        return CatalogPayloadFormat::PipeRows;
    }

    if (
        headerLine.find(',') != std::string_view::npos
        && containsCaseInsensitiveToken(headerLine, "ra")
        && containsCaseInsensitiveToken(headerLine, "dec")
        && containsCaseInsensitiveToken(headerLine, "mag")
    ) {
        return CatalogPayloadFormat::HygCsv;
    }

    return CatalogPayloadFormat::Unknown;
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> parseCatalogPayload(
    const QByteArray& payload,
    const skygate::ephemeris::HygParseProgressCallback& progressCallback
)
{
    switch (detectCatalogPayloadFormat(payload)) {
    case CatalogPayloadFormat::PipeRows:
    {
        const std::string_view rows(payload.constData(), static_cast<std::size_t>(payload.size()));
        return skygate::ephemeris::createStarCatalogFromRows(rows);
    }
    case CatalogPayloadFormat::HygCsv:
        return parseHygCsvWithQt(payload, progressCallback);
    case CatalogPayloadFormat::HygCsvGzip:
    {
        const auto uncompressedPayload = gunzipPayload(payload);
        if (!uncompressedPayload.has_value()) {
            return nullptr;
        }
        return parseHygCsvWithQt(*uncompressedPayload, progressCallback);
    }
    case CatalogPayloadFormat::Unknown:
        break;
    }

    // Fallback path for ambiguous payloads.
    std::unique_ptr<skygate::ephemeris::IStarCatalog> downloadedCatalog = parseHygCsvWithQt(
        payload,
        progressCallback
    );
    if (downloadedCatalog != nullptr) {
        return downloadedCatalog;
    }

    const auto uncompressedPayload = gunzipPayload(payload);
    if (uncompressedPayload.has_value()) {
        downloadedCatalog = parseHygCsvWithQt(*uncompressedPayload, progressCallback);
        if (downloadedCatalog != nullptr) {
            return downloadedCatalog;
        }
    }

    const std::string_view rows(payload.constData(), static_cast<std::size_t>(payload.size()));
    return skygate::ephemeris::createStarCatalogFromRows(rows);
}

void parseCatalogPayloadAsync(
    const QByteArray payload,
    QObject* callbackContext,
    std::function<void(std::size_t)> progressHandler,
    std::function<void(std::unique_ptr<skygate::ephemeris::IStarCatalog>)> completionHandler
)
{
    QPointer<QObject> safeContext(callbackContext);
    std::thread([payload, safeContext, progressHandler = std::move(progressHandler), completionHandler = std::move(completionHandler)]() mutable {
        const auto reportProgress = [&safeContext, &progressHandler](const std::size_t parsedObjectCount) {
            if (!progressHandler) {
                return;
            }
            QObject* const contextObject = safeContext.data();
            if (contextObject == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                contextObject,
                [progressHandler, parsedObjectCount]() mutable {
                    progressHandler(parsedObjectCount);
                },
                Qt::QueuedConnection
            );
        };

        std::unique_ptr<skygate::ephemeris::IStarCatalog> parsedCatalog = parseCatalogPayload(
            payload,
            reportProgress
        );
        QObject* const contextObject = safeContext.data();
        if (contextObject == nullptr) {
            return;
        }

        auto* parsedCatalogRaw = parsedCatalog.release();
        const bool scheduled = QMetaObject::invokeMethod(
            contextObject,
            [completionHandler = std::move(completionHandler), parsedCatalogRaw]() mutable {
                std::unique_ptr<skygate::ephemeris::IStarCatalog> parsedCatalogGuard(parsedCatalogRaw);
                completionHandler(std::move(parsedCatalogGuard));
            },
            Qt::QueuedConnection
        );
        if (!scheduled) {
            delete parsedCatalogRaw;
        }
    }).detach();
}

QStringList normalizedCandidateUrls(const QStringList& urlTexts)
{
    QStringList candidateUrls;
    candidateUrls.reserve(urlTexts.size());
    for (const QString& urlText : urlTexts) {
        const QString trimmed = urlText.trimmed();
        if (!trimmed.isEmpty()) {
            candidateUrls.push_back(trimmed);
        }
    }
    return candidateUrls;
}

}  // namespace

CatalogCoordinator::CatalogCoordinator(QNetworkAccessManager* networkAccessManager)
    : m_networkAccessManager(networkAccessManager)
{
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> CatalogCoordinator::ensureCoreSolarSystemBodies(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog
)
{
    if (catalog == nullptr) {
        return nullptr;
    }

    std::vector<skygate::ephemeris::CelestialBody> mergedBodies = catalog->bodies();
    const std::size_t starCount = static_cast<std::size_t>(std::count_if(
        mergedBodies.begin(),
        mergedBodies.end(),
        [](const auto& body) {
            return body.type == skygate::ephemeris::CelestialBodyType::Star;
        }
    ));

    std::unique_ptr<skygate::ephemeris::IStarCatalog> bundledCatalog =
        skygate::ephemeris::createBundledStarCatalog();
    if (bundledCatalog == nullptr) {
        return catalog;
    }

    bool injectedCoreBody = false;
    for (const auto& body : bundledCatalog->bodies()) {
        const bool isReferenceLineStar = body.type == skygate::ephemeris::CelestialBodyType::Star
            && isReferenceLineStarId(body.id)
            && starCount == 0U;
        if (
            !isSunOrMoonType(body.type)
            && body.type != skygate::ephemeris::CelestialBodyType::Planet
            && !isReferenceLineStar
        ) {
            continue;
        }

        if (containsBodyIdCaseInsensitive(mergedBodies, body.id)) {
            continue;
        }

        mergedBodies.push_back(body);
        injectedCoreBody = true;
    }

    if (!injectedCoreBody) {
        return catalog;
    }

    return std::make_unique<InMemoryStarCatalog>(std::move(mergedBodies));
}

void CatalogCoordinator::downloadCatalogFromUrls(
    const QStringList& urlTexts,
    QObject* callbackContext,
    StatusHandler statusHandler,
    CompletionHandler completionHandler
) const
{
    const QStringList candidateUrls = normalizedCandidateUrls(urlTexts);

    if (candidateUrls.isEmpty()) {
        DownloadResult result;
        result.errorText = "Catalog: Invalid URL";
        completionHandler(std::move(result));
        return;
    }

    if (m_networkAccessManager == nullptr) {
        DownloadResult result;
        result.errorText = "Catalog: Network unavailable";
        completionHandler(std::move(result));
        return;
    }

    const auto lastErrorText = std::make_shared<QString>("Catalog: Download failed");
    const auto tryDownloadNextUrl = std::make_shared<std::function<void(int)>>();
    *tryDownloadNextUrl = [this, candidateUrls, callbackContext, statusHandler, completionHandler, lastErrorText, tryDownloadNextUrl](const int index) {
        if (index >= candidateUrls.size()) {
            DownloadResult result;
            result.errorText = *lastErrorText;
            completionHandler(std::move(result));
            return;
        }

        const QUrl url = QUrl::fromUserInput(candidateUrls[index]);
        if (!url.isValid() || url.scheme().isEmpty()) {
            *lastErrorText = QString("Catalog: Invalid source URL %1").arg(candidateUrls[index]);
            (*tryDownloadNextUrl)(index + 1);
            return;
        }

        if (statusHandler) {
            statusHandler(QString("Catalog: Downloading %1 (%2/%3)").arg(
                url.toString(),
                QString::number(index + 1),
                QString::number(candidateUrls.size())
            ));
        }

        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setTransferTimeout(300000);
        request.setRawHeader("User-Agent", "Skygate/1.0");
        request.setRawHeader("Accept", "text/plain,text/csv,application/gzip,application/octet-stream,*/*");

        QNetworkReply* reply = m_networkAccessManager->get(request);
        QObject* const contextObject = callbackContext != nullptr
            ? callbackContext
            : static_cast<QObject*>(m_networkAccessManager);
        QObject::connect(
            reply,
            &QNetworkReply::finished,
            contextObject,
            [reply, contextObject, index, candidateUrls, statusHandler, completionHandler, lastErrorText, tryDownloadNextUrl]() {
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError) {
                    const int httpStatusCode =
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    *lastErrorText = QString("Catalog: Source %1 failed (%2, HTTP %3)").arg(
                        candidateUrls[index],
                        reply->errorString(),
                        QString::number(httpStatusCode)
                    );
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*tryDownloadNextUrl)(index + 1);
                    return;
                }

                const QByteArray payload = reply->readAll();
                if (payload.isEmpty()) {
                    *lastErrorText = QString("Catalog: Source %1 returned empty data").arg(candidateUrls[index]);
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*tryDownloadNextUrl)(index + 1);
                    return;
                }

                if (static_cast<std::size_t>(payload.size()) > kMaxDownloadedCatalogBytes) {
                    *lastErrorText = QString("Catalog: Source %1 file too large (max 128 MiB)").arg(
                        candidateUrls[index]
                    );
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*tryDownloadNextUrl)(index + 1);
                    return;
                }

                if (statusHandler) {
                    statusHandler("Catalog: Processing downloaded catalog...");
                }

                parseCatalogPayloadAsync(
                    payload,
                    contextObject,
                    [statusHandler](const std::size_t parsedObjectCount) {
                        if (!statusHandler) {
                            return;
                        }
                        const QString countText =
                            QLocale::system().toString(static_cast<qulonglong>(parsedObjectCount));
                        statusHandler(QString("Catalog: Processing... %1 objects parsed").arg(countText));
                    },
                    [index, candidateUrls, statusHandler, completionHandler, lastErrorText, tryDownloadNextUrl](
                        std::unique_ptr<skygate::ephemeris::IStarCatalog> downloadedCatalog
                    ) mutable {
                        if (downloadedCatalog == nullptr) {
                            *lastErrorText = QString(
                                "Catalog: Source %1 parse failed (supported: pipe rows, HYG CSV, or HYG .csv.gz)"
                            ).arg(candidateUrls[index]);
                            if (statusHandler) {
                                statusHandler(*lastErrorText);
                            }
                            (*tryDownloadNextUrl)(index + 1);
                            return;
                        }

                        DownloadResult result;
                        result.catalog = std::move(downloadedCatalog);
                        completionHandler(std::move(result));
                    }
                );
            }
        );
    };

    (*tryDownloadNextUrl)(0);
}

void CatalogCoordinator::downloadRawDataFromUrls(
    const QStringList& urlTexts,
    QObject* callbackContext,
    StatusHandler statusHandler,
    RawCompletionHandler completionHandler
) const
{
    const QStringList candidateUrls = normalizedCandidateUrls(urlTexts);

    if (candidateUrls.isEmpty()) {
        RawDownloadResult result;
        result.errorText = "Catalog: Invalid URL";
        completionHandler(std::move(result));
        return;
    }

    if (m_networkAccessManager == nullptr) {
        RawDownloadResult result;
        result.errorText = "Catalog: Network unavailable";
        completionHandler(std::move(result));
        return;
    }

    const auto lastErrorText = std::make_shared<QString>("Catalog: Download failed");
    const auto tryDownloadNextUrl = std::make_shared<std::function<void(int)>>();
    *tryDownloadNextUrl = [this, candidateUrls, callbackContext, statusHandler, completionHandler, lastErrorText, tryDownloadNextUrl](const int index) {
        if (index >= candidateUrls.size()) {
            RawDownloadResult result;
            result.errorText = *lastErrorText;
            completionHandler(std::move(result));
            return;
        }

        const QUrl url = QUrl::fromUserInput(candidateUrls[index]);
        if (!url.isValid() || url.scheme().isEmpty()) {
            *lastErrorText = QString("Catalog: Invalid source URL %1").arg(candidateUrls[index]);
            (*tryDownloadNextUrl)(index + 1);
            return;
        }

        if (statusHandler) {
            statusHandler(QString("Catalog: Downloading %1 (%2/%3)").arg(
                url.toString(),
                QString::number(index + 1),
                QString::number(candidateUrls.size())
            ));
        }

        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setTransferTimeout(300000);
        request.setRawHeader("User-Agent", "Skygate/1.0");
        request.setRawHeader("Accept", "text/plain,text/csv,application/gzip,application/octet-stream,*/*");

        QNetworkReply* reply = m_networkAccessManager->get(request);
        QObject* const contextObject = callbackContext != nullptr ? callbackContext : reply;
        QObject::connect(
            reply,
            &QNetworkReply::finished,
            contextObject,
            [reply, index, candidateUrls, statusHandler, completionHandler, lastErrorText, tryDownloadNextUrl]() {
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError) {
                    const int httpStatusCode =
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    *lastErrorText = QString("Catalog: Source %1 failed (%2, HTTP %3)").arg(
                        candidateUrls[index],
                        reply->errorString(),
                        QString::number(httpStatusCode)
                    );
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*tryDownloadNextUrl)(index + 1);
                    return;
                }

                const QByteArray payload = reply->readAll();
                if (payload.isEmpty()) {
                    *lastErrorText = QString("Catalog: Source %1 returned empty data").arg(candidateUrls[index]);
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*tryDownloadNextUrl)(index + 1);
                    return;
                }

                if (static_cast<std::size_t>(payload.size()) > kMaxDownloadedCatalogBytes) {
                    *lastErrorText = QString("Catalog: Source %1 file too large (max 128 MiB)").arg(
                        candidateUrls[index]
                    );
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*tryDownloadNextUrl)(index + 1);
                    return;
                }

                RawDownloadResult result;
                result.payload = payload;
                result.sourceUrl = candidateUrls[index];
                completionHandler(std::move(result));
            }
        );
    };

    (*tryDownloadNextUrl)(0);
}
