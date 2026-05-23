#include "catalog/io/DelimitedCatalogParser.hpp"
#include "catalog/io/DelimitedCatalogReader.hpp"

#include <QLoggingCategory>
#include <QString>
#include <QStringList>

#include <utility>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

struct InvalidRowCategory {
    std::string_view logLabel;
    std::size_t count = 0;
    QStringList samples;
};

}  // namespace

CatalogBodyParseResult DelimitedCatalogParser::run(
    const std::string_view payload,
    const DelimitedCatalogReaderOptions& readerOptions,
    const DelimitedCatalogParserOptions& runnerOptions,
    const std::span<const std::string_view> invalidCategoryLabels,
    const RowMapper& rowMapper,
    const HygParseProgressCallback& progressCallback
)
{
    std::size_t parsedObjectCount = 0;
    std::size_t dataRowNumber = 0;
    std::vector<InvalidRowCategory> categories;
    categories.reserve(invalidCategoryLabels.size());
    for (const auto& label : invalidCategoryLabels) {
        categories.push_back(InvalidRowCategory{.logLabel = label});
    }

    CatalogBodyParseResult result = DelimitedCatalogReader::read(
        payload, readerOptions, [&](const DelimitedCatalogRow& row, CatalogBodyParseResult& rowResult) {
            ++dataRowNumber;
            const RowParseOutcome outcome = rowMapper(row, dataRowNumber);

            if (outcome.body.has_value()) {
                ++parsedObjectCount;
                if (progressCallback && (parsedObjectCount % runnerOptions.progressInterval) == 0U) {
                    progressCallback(parsedObjectCount);
                }
                rowResult.bodies.push_back(std::move(*outcome.body));
                return true;
            }

            if (outcome.invalidCategoryIndex < 0
                || static_cast<std::size_t>(outcome.invalidCategoryIndex) >= categories.size()) {
                return true;
            }

            auto& category = categories[static_cast<std::size_t>(outcome.invalidCategoryIndex)];
            ++category.count;
            if (category.samples.size() < static_cast<qsizetype>(runnerOptions.maxInvalidRowSamples)) {
                category.samples.push_back(outcome.invalidSample);
            }
            return true;
        }
    );

    if (!result.isSuccess()) {
        qCWarning(skygateCatalogParseLog).noquote()
            << QString::fromStdString(runnerOptions.formatName + " parse failed:")
            << QString::fromStdString(result.errorDetail);
        return result;
    }

    for (const auto& category : categories) {
        if (category.count > 0U) {
            qCWarning(skygateCatalogParseLog).noquote()
                << QString::fromStdString(runnerOptions.formatName) << "skipped"
                << static_cast<qulonglong>(category.count)
                << QString::fromStdString(std::string("rows with ") + std::string(category.logLabel) + "; samples:")
                << category.samples.join(QStringLiteral("; "));
        }
    }

    if (progressCallback) {
        progressCallback(parsedObjectCount);
    }

    if (parsedObjectCount == 0U) {
        result.errorCode = runnerOptions.zeroResultCode;
        result.errorDetail = runnerOptions.zeroResultDetail;
        qCWarning(skygateCatalogParseLog).noquote()
            << QString::fromStdString(runnerOptions.formatName + " parse failed:")
            << QString::fromStdString(result.errorDetail);
        return result;
    }

    result.diagnostics.parsedBodyCount = parsedObjectCount;
    return result;
}

}  // namespace skygate::ephemeris
