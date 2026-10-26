#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/IStarCatalog.hpp"
#include "catalog/SkyCatalogRuntime.hpp"
#include "support/CatalogRuntimeFixture.hpp"

#include <QLocale>
#include <QtTest/QtTest>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using skygate::ui::tests::CatalogRuntimeFixture;

namespace {

std::optional<std::size_t>
runtimeBodyIndexById(const skygate::ui::internal::SkyCatalogRuntime& runtime, const std::string& id)
{
    const skygate::ephemeris::IStarCatalog* catalog = runtime.starCatalog();
    if (catalog == nullptr) {
        return std::nullopt;
    }

    const auto bodies = catalog->bodies();
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index] != nullptr && bodies[index]->id == id) {
            return index;
        }
    }
    return std::nullopt;
}

}  // namespace

class SkyCatalogRuntimePresentationTests final : public QObject {
    Q_OBJECT

private slots:
    void bundledBrightStarsCarryDataProvenance();
    void bundledDeepSkyFallbackKeepsValuesAndFillsGaps();
    void explicitReplacementSourcesKeepVisibleOrder();
    void bundledFallbackParticipationFollowsSourceState();
    void statusSummarizesEnabledCollectionParticipation();
    void bundledSourceNamesComeFromActiveParticipation();
};

void SkyCatalogRuntimePresentationTests::bundledBrightStarsCarryDataProvenance()
{
    skygate::ephemeris::OwnGalaxyCelestialBody constellation;
    constellation.id = "constellation_orion";
    constellation.displayName = "Orion";
    constellation.kind = skygate::ephemeris::BaseCelestialBody::Kind::Constellation;
    auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({std::move(constellation)});
    QVERIFY(catalog != nullptr);

    skygate::ui::internal::SkyCatalogRuntime runtime(std::move(catalog));
    static_cast<void>(runtime.initialize({}));

    const auto bodies = runtime.starCatalog()->bodies();
    bool sawSirius = false;
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        const skygate::ephemeris::BaseCelestialBody* body = bodies[index];
        if (body == nullptr) {
            continue;
        }
        if (body->id == "sirius") {
            sawSirius = true;
            QCOMPARE(runtime.sourceIds()[index], QStringLiteral("bundled-core"));
            QCOMPARE(runtime.sourceTitle(QStringLiteral("bundled-core")), QStringLiteral("Bundled core"));
        }
    }
    QVERIFY(sawSirius);
    QVERIFY(!runtime.sourceTitles().contains(QStringLiteral("built-in-ephemeris")));
}

void SkyCatalogRuntimePresentationTests::bundledDeepSkyFallbackKeepsValuesAndFillsGaps()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
    static_cast<void>(
        runtime.initialize(CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kFallbackBundledDeepSky))
    );

    // A mixed configured source (stars and DSOs) keeps its own values without
    // being relabeled DeepSkyOnly to suppress the bundled fallback.
    const auto applyResult = runtime.applySource(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("mixed"),
            .title = QStringLiteral("Mixed"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = CatalogRuntimeFixture::makeMixedCatalog(
                {CatalogRuntimeFixture::makeFixedBody("mixed_star", "Mixed Star")},
                {CatalogRuntimeFixture::makeMessierBody("messier_031", "31", 0.0)}
            ),
            .foundObjectCount = 0,
        },
        CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kFallbackBundledDeepSky)
    );
    QVERIFY(applyResult.catalogChanged);
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "mixed_star"));

    // The configured M31 magnitude survives the falling-back bundled M31
    // (3.44): the fallback fills gaps instead of replacing later.
    const auto m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.starCatalog()->bodies()[*m31Index]->visualMagnitude, 0.0);
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("mixed"));
    QCOMPARE(runtime.contributorSourceIds()[*m31Index], QStringList({QStringLiteral("mixed")}));

    // The fallback supplies the Messier identities the configured source
    // lacks, and each contribution carries its own provenance.
    const auto m1Index = runtimeBodyIndexById(runtime, "messier_001");
    QVERIFY(m1Index.has_value());
    QCOMPARE(runtime.sourceIds()[*m1Index], QStringLiteral("bundled-deep-sky"));
    QVERIFY(runtime.sourceTitles().contains(QStringLiteral("bundled-deep-sky")));
    QCOMPARE(runtime.sourceTitle(QStringLiteral("bundled-deep-sky")), QStringLiteral("Bundled Messier"));

    // Bundled core augmentation still supplies the required Sun/Moon/planet
    // bodies that no configured source provides.
    const auto marsIndex = runtimeBodyIndexById(runtime, "mars");
    QVERIFY(marsIndex.has_value());
    QCOMPARE(runtime.sourceIds()[*marsIndex], QStringLiteral("bundled-core"));
}

void SkyCatalogRuntimePresentationTests::explicitReplacementSourcesKeepVisibleOrder()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
    static_cast<void>(
        runtime.initialize(CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kFallbackBundledDeepSky))
    );

    const auto applyM31 = [&](const QString& instanceId,
                              const skygate::ephemeris::CatalogCompositionPolicy policy,
                              const double magnitude) {
        return runtime.applySource(
            skygate::ui::internal::SkyCatalogSourceRecord{
                .instanceId = instanceId,
                .title = instanceId,
                .version = QString(),
                .policy = policy,
                .enabled = true,
                .catalog = CatalogRuntimeFixture::makeMixedCatalog(
                    {}, {CatalogRuntimeFixture::makeMessierBody("messier_031", "31", magnitude)}
                ),
                .foundObjectCount = 0,
            },
            CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kFallbackBundledDeepSky)
        );
    };

    QVERIFY(
        applyM31(QStringLiteral("merge-a"), skygate::ephemeris::CatalogCompositionPolicy::Merge, 0.0).catalogChanged
    );
    QVERIFY(
        applyM31(QStringLiteral("dso"), skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly, 5.0).catalogChanged
    );

    // The explicit later replacement source wins; the fallback never overrides
    // an explicit source's value.
    auto m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.starCatalog()->bodies()[*m31Index]->visualMagnitude, 5.0);
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("dso"));

    // Moving the replacement source before the merge source restores the merge
    // source's value: the visible collection order decides.
    QVERIFY(runtime
                .moveSource(
                    QStringLiteral("dso"),
                    1U,
                    CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kFallbackBundledDeepSky)
                )
                .catalogChanged);
    m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.starCatalog()->bodies()[*m31Index]->visualMagnitude, 0.0);
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("merge-a"));

    // The fallback still fills the identities no configured source supplies.
    const auto m1Index = runtimeBodyIndexById(runtime, "messier_001");
    QVERIFY(m1Index.has_value());
    QCOMPARE(runtime.sourceIds()[*m1Index], QStringLiteral("bundled-deep-sky"));
}

void SkyCatalogRuntimePresentationTests::bundledFallbackParticipationFollowsSourceState()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
    static_cast<void>(
        runtime.initialize(CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kFallbackBundledDeepSky))
    );

    const auto applySource = [&](const QString& instanceId,
                                 const skygate::ephemeris::CatalogCompositionPolicy policy,
                                 std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog) {
        return runtime.applySource(
            skygate::ui::internal::SkyCatalogSourceRecord{
                .instanceId = instanceId,
                .title = instanceId,
                .version = QString(),
                .policy = policy,
                .enabled = true,
                .catalog = std::move(catalog),
                .foundObjectCount = 0,
            },
            CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kFallbackBundledDeepSky)
        );
    };

    // An enabled explicit DSO source replaces the fallback at its visible
    // position.
    QVERIFY(applySource(
                QStringLiteral("dso"),
                skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
                CatalogRuntimeFixture::makeMixedCatalog(
                    {}, {CatalogRuntimeFixture::makeMessierBody("messier_031", "31", 5.0)}
                )
    )
                .catalogChanged);
    auto m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.starCatalog()->bodies()[*m31Index]->visualMagnitude, 5.0);
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("dso"));

    // Disabling the source lets the configured fallback fill the missing
    // identity, visible through the collection's participation information.
    QVERIFY(runtime
                .setSourceEnabled(
                    QStringLiteral("dso"),
                    false,
                    CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kFallbackBundledDeepSky)
                )
                .catalogChanged);
    m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.starCatalog()->bodies()[*m31Index]->visualMagnitude, 3.44);
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("bundled-deep-sky"));

    // Re-enabling restores the explicit source's value.
    QVERIFY(runtime
                .setSourceEnabled(
                    QStringLiteral("dso"),
                    true,
                    CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kFallbackBundledDeepSky)
                )
                .catalogChanged);
    m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.starCatalog()->bodies()[*m31Index]->visualMagnitude, 5.0);

    // Removing it lets the fallback fill the identity again.
    QVERIFY(runtime
                .removeSource(
                    QStringLiteral("dso"),
                    CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kFallbackBundledDeepSky)
                )
                .catalogChanged);
    m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("bundled-deep-sky"));

    // An enabled source with no deep-sky rows cannot suppress the fallback.
    QVERIFY(applySource(
                QStringLiteral("dso-empty"),
                skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
                CatalogRuntimeFixture::makeMixedCatalog(
                    {CatalogRuntimeFixture::makeFixedBody("dso_empty_star", "Not Deep Sky")}, {}
                )
    )
                .catalogChanged);
    m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("bundled-deep-sky"));

    // With bundled deep-sky participation disabled, nothing fills the gap.
    QVERIFY(runtime
                .rebuildActiveCatalog(
                    CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kDisabledBundledDeepSky)
                )
                .catalogChanged);
    QVERIFY(!CatalogRuntimeFixture::runtimeContainsBody(runtime, "messier_031"));
}

void SkyCatalogRuntimePresentationTests::statusSummarizesEnabledCollectionParticipation()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
    const auto options = CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kDisabledBundledDeepSky);
    const auto initializeResult = runtime.initialize(options);
    QVERIFY(initializeResult.statusTextChanged);

    // One enabled configured source: the summary names it and the counts come
    // from the active snapshot. The bundled deep-sky fallback is disabled, so
    // nothing claims its data.
    QVERIFY(initializeResult.statusText.startsWith(QStringLiteral("Catalog: Bundled + Bundled core (")));
    QVERIFY(!initializeResult.statusText.contains(QStringLiteral("Bundled Messier")));

    const auto applyConfiguredSource = [&](const QString& instanceId,
                                           const QString& title,
                                           const skygate::ephemeris::CatalogCompositionPolicy policy,
                                           std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog) {
        return runtime.applySource(
            skygate::ui::internal::SkyCatalogSourceRecord{
                .instanceId = instanceId,
                .title = title,
                .version = QString(),
                .policy = policy,
                .enabled = true,
                .catalog = std::move(catalog),
                .foundObjectCount = 0,
            },
            options
        );
    };

    // Three mixed enabled sources: two star sources and one deep-sky source.
    QVERIFY(applyConfiguredSource(
                QStringLiteral("source-alpha"),
                QStringLiteral("Alpha"),
                skygate::ephemeris::CatalogCompositionPolicy::Merge,
                CatalogRuntimeFixture::makeSingleStarCatalog("alpha_1", "Alpha Star")
    )
                .catalogChanged);
    QVERIFY(applyConfiguredSource(
                QStringLiteral("source-beta"),
                QStringLiteral("Beta"),
                skygate::ephemeris::CatalogCompositionPolicy::Merge,
                CatalogRuntimeFixture::makeSingleStarCatalog("beta_1", "Beta Star")
    )
                .catalogChanged);
    const auto gammaResult = applyConfiguredSource(
        QStringLiteral("source-gamma"),
        QStringLiteral("Gamma"),
        skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
        CatalogRuntimeFixture::makeMixedCatalog({}, {CatalogRuntimeFixture::makeMessierBody("messier_031", "31", 1.0)})
    );
    QVERIFY(gammaResult.catalogChanged);

    // Every enabled source is named in visible collection order, and the status
    // text carries the summary plus the snapshot counts.
    QVERIFY(runtime.participationSummary().startsWith(QStringLiteral("Bundled + Alpha + Beta + Gamma")));
    QVERIFY(gammaResult.statusText.startsWith(QStringLiteral("Catalog: %1 (").arg(runtime.participationSummary())));
    const QLocale locale = QLocale::system();
    QVERIFY(
        gammaResult.statusText.endsWith(QStringLiteral("%1 deep sky, %2 constellations)")
                                            .arg(
                                                locale.toString(static_cast<qulonglong>(runtime.deepSkyObjectCount())),
                                                locale.toString(static_cast<qulonglong>(runtime.constellationCount()))
                                            ))
    );
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "alpha_1"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "beta_1"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "messier_031"));

    // Disabling a source removes it from the summary and from the counts
    // without changing the other sources' participation.
    const auto disableGammaResult = runtime.setSourceEnabled(QStringLiteral("source-gamma"), false, options);
    QVERIFY(disableGammaResult.catalogChanged);
    QCOMPARE(runtime.participationSummary(), QStringLiteral("Bundled + Alpha + Beta + Bundled core"));
    QCOMPARE(runtime.deepSkyObjectCount(), 0U);
    QVERIFY(disableGammaResult.statusText.contains(QStringLiteral("0 deep sky")));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "alpha_1"));

    // Re-enabling restores the source's participation.
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("source-gamma"), true, options).catalogChanged);
    QVERIFY(runtime.participationSummary().contains(QStringLiteral("Gamma")));

    // Reordering the collection reorders the summary: the collection order is
    // the precedence order.
    QVERIFY(runtime.moveSource(QStringLiteral("source-gamma"), 1U, options).catalogChanged);
    QVERIFY(runtime.participationSummary().startsWith(QStringLiteral("Bundled + Gamma + Alpha + Beta")));

    // The leading label follows the enabled collection instead of the first
    // configured record.
    QCOMPARE(runtime.sourceLabel(), QStringLiteral("Bundled"));
    QVERIFY(runtime.moveSource(QStringLiteral("source-beta"), 0U, options).catalogChanged);
    QCOMPARE(runtime.sourceLabel(), QStringLiteral("Beta"));

    // Removing a source drops its title and its objects.
    QVERIFY(runtime.removeSource(QStringLiteral("source-alpha"), options).catalogChanged);
    QVERIFY(!runtime.participationSummary().contains(QStringLiteral("Alpha")));
    QVERIFY(!CatalogRuntimeFixture::runtimeContainsBody(runtime, "alpha_1"));

    // With every configured source disabled only the implicit bundled
    // augmentation is left, and no disabled configured source is named.
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("source-beta"), false, options).catalogChanged);
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("source-gamma"), false, options).catalogChanged);
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("primary"), false, options).catalogChanged);
    QCOMPARE(runtime.participationSummary(), QStringLiteral("Bundled core"));
    QCOMPARE(runtime.sourceLabel(), QStringLiteral("Bundled core"));
}

void SkyCatalogRuntimePresentationTests::bundledSourceNamesComeFromActiveParticipation()
{
    std::unique_ptr<skygate::ephemeris::IStarCatalog> bundledCatalog =
        skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    QVERIFY(bundledCatalog != nullptr);

    skygate::ui::internal::SkyCatalogRuntime runtime(std::move(bundledCatalog));
    const auto fallbackOptions =
        CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kFallbackBundledDeepSky);
    const auto initializeResult = runtime.initialize(fallbackOptions);
    QVERIFY(initializeResult.catalogChanged);

    // The bundled source already supplies the bundled deep-sky objects, so the
    // enabled fallback contributes nothing and is not named.
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "messier_031"));
    QVERIFY(!runtime.participationSummary().contains(QStringLiteral("Bundled Messier")));
    QVERIFY(!runtime.sourceTitles().contains(QStringLiteral("bundled-deep-sky")));
    QVERIFY(!initializeResult.statusText.contains(QStringLiteral("Bundled Messier")));
    const auto bundledM31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(bundledM31Index.has_value());
    QCOMPARE(runtime.sourceIds()[*bundledM31Index], QStringLiteral("primary"));

    // Replacing the bundled star source with a star-only catalog lets the
    // fallback supply the deep-sky objects, and the summary names it from the
    // recorded provenance instead of a catalog-name default.
    QVERIFY(runtime
                .applySource(
                    skygate::ui::internal::SkyCatalogSourceRecord{
                        .instanceId = QStringLiteral("stars"),
                        .title = QStringLiteral("Stars"),
                        .version = QString(),
                        .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
                        .enabled = true,
                        .catalog = CatalogRuntimeFixture::makeSingleStarCatalog("stars_1", "Configured Star"),
                        .foundObjectCount = 0,
                    },
                    fallbackOptions
                )
                .catalogChanged);
    QVERIFY(runtime.removeSource(QStringLiteral("primary"), fallbackOptions).catalogChanged);
    QCOMPARE(runtime.sourceTitle(QStringLiteral("bundled-deep-sky")), QStringLiteral("Bundled Messier"));
    QCOMPARE(runtime.participationSummary(), QStringLiteral("Stars + Bundled core + Bundled Messier"));
    const auto fallbackM31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(fallbackM31Index.has_value());
    QCOMPARE(runtime.sourceIds()[*fallbackM31Index], QStringLiteral("bundled-deep-sky"));

    // Turning the fallback participation off drops its name together with its
    // objects.
    QVERIFY(runtime
                .rebuildActiveCatalog(
                    CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kDisabledBundledDeepSky)
                )
                .catalogChanged);
    QVERIFY(!runtime.participationSummary().contains(QStringLiteral("Bundled Messier")));
    QVERIFY(!CatalogRuntimeFixture::runtimeContainsBody(runtime, "messier_031"));

    // A configured source the user disabled is never presented as the active
    // participant.
    QVERIFY(runtime
                .setSourceEnabled(
                    QStringLiteral("stars"),
                    false,
                    CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kDisabledBundledDeepSky)
                )
                .catalogChanged);
    QCOMPARE(runtime.participationSummary(), QStringLiteral("Bundled core"));
}

QTEST_APPLESS_MAIN(SkyCatalogRuntimePresentationTests)

#include "SkyCatalogRuntimePresentationTests.moc"
