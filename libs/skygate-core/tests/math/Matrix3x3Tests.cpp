#include "math/Matrix3x3.hpp"

#include <QtTest/QtTest>

#include <cmath>

namespace {

[[nodiscard]] bool isNear(const double value, const double expected, const double tolerance = 1e-12)
{
    return std::abs(value - expected) <= tolerance;
}

void compareVector(const skygate::core::Vector3d& actual, const skygate::core::Vector3d& expected)
{
    QVERIFY(isNear(actual.x, expected.x));
    QVERIFY(isNear(actual.y, expected.y));
    QVERIFY(isNear(actual.z, expected.z));
}

}  // namespace

class Matrix3x3Tests final : public QObject {
    Q_OBJECT

private slots:
    void identityLeavesVectorsUnchanged();
    void preservesRowsAndValues();
    void multipliesVectorUsingRows();
    void multipliesVectorUsingTransposedRows();
    void transposesRowsAndColumns();
};

void Matrix3x3Tests::identityLeavesVectorsUnchanged()
{
    const skygate::core::Vector3d vector{.x = 4.0, .y = -2.5, .z = 8.0};
    const skygate::core::Matrix3x3 matrix = skygate::core::Matrix3x3::identity();

    compareVector(matrix.multiplied(vector), vector);
    compareVector(matrix.transposeMultiplied(vector), vector);
}

void Matrix3x3Tests::preservesRowsAndValues()
{
    const skygate::core::Matrix3x3 matrix{
        {.x = 1.0, .y = 2.0, .z = 3.0},
        {.x = 4.0, .y = 5.0, .z = 6.0},
        {.x = 7.0, .y = 8.0, .z = 9.0},
    };

    compareVector(matrix.row(0U), {.x = 1.0, .y = 2.0, .z = 3.0});
    compareVector(matrix.row(1U), {.x = 4.0, .y = 5.0, .z = 6.0});
    compareVector(matrix.row(2U), {.x = 7.0, .y = 8.0, .z = 9.0});
    QCOMPARE(matrix.value(0U, 2U), 3.0);
    QCOMPARE(matrix.value(2U, 1U), 8.0);
}

void Matrix3x3Tests::multipliesVectorUsingRows()
{
    const skygate::core::Matrix3x3 matrix{
        {.x = 1.0, .y = 2.0, .z = 3.0},
        {.x = 4.0, .y = 5.0, .z = 6.0},
        {.x = 7.0, .y = 8.0, .z = 9.0},
    };

    compareVector(matrix.multiplied({.x = 2.0, .y = 3.0, .z = 4.0}), {.x = 20.0, .y = 47.0, .z = 74.0});
}

void Matrix3x3Tests::multipliesVectorUsingTransposedRows()
{
    const skygate::core::Matrix3x3 matrix{
        {.x = 1.0, .y = 2.0, .z = 3.0},
        {.x = 4.0, .y = 5.0, .z = 6.0},
        {.x = 7.0, .y = 8.0, .z = 9.0},
    };

    compareVector(matrix.transposeMultiplied({.x = 2.0, .y = 3.0, .z = 4.0}), {.x = 42.0, .y = 51.0, .z = 60.0});
}

void Matrix3x3Tests::transposesRowsAndColumns()
{
    const skygate::core::Matrix3x3 matrix{
        {.x = 1.0, .y = 2.0, .z = 3.0},
        {.x = 4.0, .y = 5.0, .z = 6.0},
        {.x = 7.0, .y = 8.0, .z = 9.0},
    };

    const skygate::core::Matrix3x3 transposed = matrix.transposed();
    compareVector(transposed.row(0U), {.x = 1.0, .y = 4.0, .z = 7.0});
    compareVector(transposed.row(1U), {.x = 2.0, .y = 5.0, .z = 8.0});
    compareVector(transposed.row(2U), {.x = 3.0, .y = 6.0, .z = 9.0});
}

QTEST_APPLESS_MAIN(Matrix3x3Tests)

#include "Matrix3x3Tests.moc"
