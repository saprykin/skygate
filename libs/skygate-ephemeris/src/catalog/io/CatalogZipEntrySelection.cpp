#include "CatalogZipEntrySelection.hpp"

namespace skygate::ephemeris {

CatalogLoadResult::ErrorCode CatalogZipEntrySelection::errorCode() const noexcept
{
    switch (status) {
    case Status::Selected:
        return CatalogLoadResult::ErrorCode::NoError;
    case Status::InvalidArchive:
    case Status::UnusableMember:
    case Status::NoSupportedMember:
    case Status::NoReadableEntry:
        return CatalogLoadResult::ErrorCode::InvalidZipData;
    case Status::MissingMember:
        return CatalogLoadResult::ErrorCode::ArchiveMemberNotFound;
    case Status::AmbiguousMember:
        return CatalogLoadResult::ErrorCode::AmbiguousArchiveMember;
    }

    return CatalogLoadResult::ErrorCode::InvalidZipData;
}

std::string CatalogZipEntrySelection::errorDetail() const
{
    switch (status) {
    case Status::Selected:
        return {};
    case Status::InvalidArchive:
        return "ZIP catalog payload could not be parsed.";
    case Status::MissingMember:
        return "ZIP catalog payload does not contain member '" + selectedPath + "'.";
    case Status::UnusableMember:
        return "ZIP catalog member '" + selectedPath + "' is not a readable catalog entry.";
    case Status::AmbiguousMember:
        return "ZIP catalog payload contains multiple supported catalog members.";
    case Status::NoSupportedMember:
        return "ZIP catalog payload does not contain a supported catalog member.";
    case Status::NoReadableEntry:
        return "ZIP catalog payload does not contain a readable CSV entry.";
    }

    return {};
}

}  // namespace skygate::ephemeris
