#pragma once

#include "EphemerisDataStaleness.hpp"
#include "EphemerisTableDataStatus.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

/// Shared skeleton of the ephemeris table loaders: the missing-asset outcome
/// of a snapshot load and the parse tail that marks stale data and builds the
/// table-backed provider. Each loader keeps its snapshot lookup, its table
/// diagnostics, and its own result type.
class EphemerisTableLoading final {
public:
    /// Loaded table: the provider that serves the parsed rows and the info
    /// that reports them. A rejected parse leaves the provider empty.
    template <typename Provider, typename Info>
    struct LoadedTable {
        std::shared_ptr<const Provider> provider;
        Info info;
    };

    EphemerisTableLoading() = delete;

    /// Outcome of a snapshot load that has no asset for its table.
    template <typename Provider, typename Info>
    [[nodiscard]] static LoadedTable<Provider, Info> missingAsset(std::string diagnosticText)
    {
        LoadedTable<Provider, Info> loaded;
        loaded.info.status = EphemerisTableDataStatus::Missing;
        loaded.info.diagnosticText = std::move(diagnosticText);
        return loaded;
    }

    /// Tail of a text-asset load: a rejected parse keeps its info, an accepted
    /// parse becomes stale for the reference epoch and turns into a
    /// table-backed provider that `makeProvider` builds from the info and rows.
    template <typename Provider, typename Info, typename Entry, typename MakeProvider>
    [[nodiscard]] static LoadedTable<Provider, Info> loadParsedTable(
        Info info,
        std::vector<Entry> entries,
        const bool parseSucceeded,
        const std::optional<skygate::core::AstronomicalEpoch>& referenceEpoch,
        std::string staleDiagnosticText,
        MakeProvider makeProvider
    )
    {
        if (!parseSucceeded) {
            return LoadedTable<Provider, Info>{.provider = {}, .info = std::move(info)};
        }

        if (EphemerisDataStaleness::isStale(referenceEpoch, info.expiresAt)) {
            info.status = EphemerisTableDataStatus::Stale;
            info.diagnosticText = std::move(staleDiagnosticText);
        }

        LoadedTable<Provider, Info> loaded;
        loaded.info = info;
        loaded.provider = makeProvider(std::move(info), std::move(entries));
        return loaded;
    }
};

}  // namespace skygate::ephemeris
