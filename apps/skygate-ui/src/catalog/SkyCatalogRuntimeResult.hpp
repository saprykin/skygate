#pragma once

#include <QString>

namespace skygate::ui::internal {

struct SkyCatalogRuntimeResult final {
    QString statusText;
    bool statusTextChanged = false;
    bool datasetInfoChanged = false;
    bool deepSkyCatalogInfoChanged = false;
    bool catalogChanged = false;
    // False when the requested transition was rejected and nothing was
    // published: statusText reports the operation error. The last accepted
    // configuration, snapshot, counts, provenance, related view, and revision
    // are unchanged.
    bool succeeded = true;
};

}  // namespace skygate::ui::internal
