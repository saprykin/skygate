#include "ZipEntryMetadata.hpp"

namespace skygate::ephemeris {

bool ZipEntryMetadata::isDirectory() const
{
    return !path.empty() && path.back() == '/';
}

bool ZipEntryMetadata::isEncrypted() const
{
    return (generalPurposeFlag & 0x1U) != 0U;
}

}  // namespace skygate::ephemeris
