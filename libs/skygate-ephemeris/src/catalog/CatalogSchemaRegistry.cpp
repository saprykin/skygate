#include "CatalogSchemaRegistry.hpp"
#include "catalog/bundled/BundledCatalogParser.hpp"
#include "catalog/hyg/HygCatalogParser.hpp"
#include "catalog/opengc/OpenNgcCatalogParser.hpp"

#include <QString>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] std::vector<CatalogSchemaDescriptor>& mutableDescriptors() noexcept
{
    static std::vector<CatalogSchemaDescriptor> descriptors = [] {
        std::vector<CatalogSchemaDescriptor> builtIn;
        builtIn.reserve(3);
        builtIn.push_back(
            CatalogSchemaDescriptor{
                .type = CatalogSourceType::Bundled,
                .diagnosticName = "bundled",
                .delimiter = ',',
                .requiredColumns = {},
                .createParser = []() { return std::make_unique<BundledCatalogParser>(); },
            }
        );
        builtIn.push_back(
            CatalogSchemaDescriptor{
                .type = CatalogSourceType::HygCsv,
                .diagnosticName = "HYG CSV",
                .delimiter = ',',
                .requiredColumns = {QStringLiteral("ra"), QStringLiteral("dec"), QStringLiteral("mag")},
                .createParser = []() { return std::make_unique<HygCatalogParser>(); },
            }
        );
        builtIn.push_back(
            CatalogSchemaDescriptor{
                .type = CatalogSourceType::OpenNgcCsv,
                .diagnosticName = "OpenNGC CSV",
                .delimiter = ';',
                .requiredColumns =
                    {
                        QStringLiteral("Name"),
                        QStringLiteral("Type"),
                        QStringLiteral("RA"),
                        QStringLiteral("Dec"),
                    },
                .createParser = []() { return std::make_unique<OpenNgcCatalogParser>(); },
            }
        );
        return builtIn;
    }();
    return descriptors;
}

}  // namespace

const std::vector<CatalogSchemaDescriptor>& CatalogSchemaRegistry::descriptors() noexcept
{
    return mutableDescriptors();
}

const CatalogSchemaDescriptor* CatalogSchemaRegistry::find(const CatalogSourceType type) noexcept
{
    const auto& all = mutableDescriptors();
    const auto it = std::find_if(all.begin(), all.end(), [type](const CatalogSchemaDescriptor& descriptor) {
        return descriptor.type == type;
    });
    return it == all.end() ? nullptr : &*it;
}

std::string CatalogSchemaRegistry::diagnosticName(const CatalogSourceType type)
{
    const CatalogSchemaDescriptor* descriptor = find(type);
    return descriptor == nullptr ? "unknown" : descriptor->diagnosticName;
}

std::unique_ptr<ICatalogParser> CatalogSchemaRegistry::createParser(const CatalogSourceType type)
{
    const CatalogSchemaDescriptor* descriptor = find(type);
    if (descriptor == nullptr || !descriptor->createParser) {
        return nullptr;
    }
    return descriptor->createParser();
}

void CatalogSchemaRegistry::registerSchema(CatalogSchemaDescriptor descriptor)
{
    mutableDescriptors().push_back(std::move(descriptor));
}

}  // namespace skygate::ephemeris
