#pragma once

#include <string_view>

namespace skygate::ephemeris::tests {

// Shared header corpus for the payload format detector, delimited reader, and
// payload parser tests. The three suites use these exact cases so header
// preparation and column recognition cannot drift between detection and
// parsing.
struct CatalogHeaderCorpus final {
    // A HYG payload with a leading full-line comment.
    static constexpr std::string_view kHygLeadingComment = "# catalog comment\n"
                                                           "id,ra,dec,mag\n"
                                                           "1,6.7525,-16.7161,-1.46\n";

    // An OpenNGC payload with a leading comment and a blank line.
    static constexpr std::string_view kOpenNgcLeadingComment = "# catalog comment\n"
                                                               "\n"
                                                               "Name;Type;RA;Dec\n"
                                                               "NGC0224;G;00:42:44.35;+41:16:08.6\n";

    // A comma-separated header whose names merely contain ra/dec/mag. It must
    // not be recognized as HYG.
    static constexpr std::string_view kHygLookalikeHeader = "id,rate,declination,magnitude\n"
                                                            "1,2,3,4\n";

    // A HYG payload with a UTF-8 BOM before the first column.
    static constexpr std::string_view kHygBom = "\xef\xbb\xbf"
                                                "id,ra,dec,mag\n"
                                                "1,6.7525,-16.7161,-1.46\n";

    // A HYG payload whose first quoted header field contains '#'. The hash is
    // data and must not be treated as a comment.
    static constexpr std::string_view kHygQuotedHash = "\"#id\",ra,dec,mag\n"
                                                       "1,6.7525,-16.7161,-1.46\n";
};

}  // namespace skygate::ephemeris::tests
