#include <QtTest/QtTest>

#include <calceph.h>
#include <zstd.h>

#include <array>

class HighPrecisionDependencySmokeTests final : public QObject {
    Q_OBJECT

private slots:
    void linksCalcephAndZstd();
};

void HighPrecisionDependencySmokeTests::linksCalcephAndZstd()
{
    std::array<char, CALCEPH_MAX_CONSTANTNAME> calcephVersion{};
    calceph_getversion_str(calcephVersion.data());

    QVERIFY(!QByteArray(calcephVersion.data()).isEmpty());
    QVERIFY(ZSTD_versionNumber() > 0U);
}

QTEST_MAIN(HighPrecisionDependencySmokeTests)

#include "HighPrecisionDependencySmokeTests.moc"
