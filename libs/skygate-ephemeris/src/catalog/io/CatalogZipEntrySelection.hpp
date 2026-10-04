#pragma once

#include <cstdint>
#include <string>

namespace skygate::ephemeris {

// Result of applying the ZIP archive member selection policy.
struct CatalogZipEntrySelection final {
    enum class Status : std::uint8_t {
        Selected,           // exactly one member was chosen; payload is set
        InvalidArchive,     // the central directory could not be parsed
        MissingMember,      // the explicit selector names no archive member
        UnusableMember,     // the explicit selector names a directory, encrypted, or unreadable member
        AmbiguousMember,    // multiple supported candidates and no explicit selector
        NoSupportedMember,  // multiple candidates but none decodes to a supported schema
        NoReadableEntry,    // no candidate could be read (empty archive or extraction failure)
    };

    Status status = Status::NoReadableEntry;
    std::string selectedPath;
    std::string payload;
};

}  // namespace skygate::ephemeris
