#include <QtTest>

#include "LocationCatalogModel.hpp"
#include "MacDockIcon.hpp"
#include "SkyTheme.hpp"

class ResourceDiagnosticsTests final : public QObject {
    Q_OBJECT

private slots:
    void packagedResourceDependenciesResolve();
    void platformNativeHooksTolerateEmptyInputs();
};

void ResourceDiagnosticsTests::packagedResourceDependenciesResolve()
{
    const skygate::ui::internal::SkyThemeRepository themeRepository;
    QVERIFY(themeRepository.themeOptions().size() >= 2);
    QVERIFY(themeRepository.defaultTheme().ui.windowBackground.isValid());

    LocationCatalogModel locations;
    QVERIFY(locations.rowCount() > 0);
    QVERIFY(!locations.index(0, 0).data(LocationCatalogModel::DisplayTextRole).toString().isEmpty());
}

void ResourceDiagnosticsTests::platformNativeHooksTolerateEmptyInputs()
{
#ifdef Q_OS_MACOS
    skygate::ui::setMacDockIcon(QString());
#else
    QSKIP("Mac dock icon hook is only built on macOS.");
#endif
}

QTEST_GUILESS_MAIN(ResourceDiagnosticsTests)

#include "ResourceDiagnosticsTests.moc"
