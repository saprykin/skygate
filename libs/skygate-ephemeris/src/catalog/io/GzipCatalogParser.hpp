#pragma once

#include "catalog/CatalogBodyParseResult.hpp"
#include "catalog/ICatalogParser.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

class GzipCatalogParser final : public ICatalogParser {
public:
    explicit GzipCatalogParser(std::unique_ptr<ICatalogParser> innerParser);

    [[nodiscard]] CatalogBodyParseResult
    parse(std::string_view data, const CatalogParseProgressCallback& progressCallback) const override;

private:
    std::unique_ptr<ICatalogParser> m_innerParser;
};

}  // namespace skygate::ephemeris
