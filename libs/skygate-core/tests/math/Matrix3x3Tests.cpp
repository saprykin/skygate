#include "math/MathConstants.hpp"
#include "math/Matrix3x3.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <cstddef>

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

void compareRow(
    const skygate::core::Matrix3x3& matrix, const std::size_t rowIndex, const skygate::core::Vector3d& expected
)
{
    QCOMPARE(matrix.value(rowIndex, 0U), expected.x);
    QCOMPARE(matrix.value(rowIndex, 1U), expected.y);
    QCOMPARE(matrix.value(rowIndex, 2U), expected.z);
}

}  // namespace

class Matrix3x3Tests final : public QObject {
    Q_OBJECT

private slots:
    void rotationZWithZeroAngleIsIdentity();
    void rotationZQuarterTurnMapsAxes();
    void storesValuesInRowMajorOrder();
    void multipliesVectorUsingRows();
    void multipliesVectorUsingTransposedRows();
    void transposeMultipliedMapsBasisVectorsToRows();
};

void Matrix3x3Tests::rotationZWithZeroAngleIsIdentity()
{
    const skygate::core::Vector3d vector{.x = 4.0, .y = -2.5, .z = 8.0};
    const skygate::core::Matrix3x3 matrix = skygate::core::Matrix3x3::rotationZ(0.0);

    compareRow(matrix, 0U, {.x = 1.0, .y = 0.0, .z = 0.0});
    compareRow(matrix, 1U, {.x = 0.0, .y = 1.0, .z = 0.0});
    compareRow(matrix, 2U, {.x = 0.0, .y = 0.0, .z = 1.0});
    compareVector(matrix.multiplied(vector), vector);
    compareVector(matrix.transposeMultiplied(vector), vector);
}

void Matrix3x3Tests::rotationZQuarterTurnMapsAxes()
{
    const skygate::core::Matrix3x3 matrix =
        skygate::core::Matrix3x3::rotationZ(skygate::core::MathConstants::kPi / 2.0);

    compareVector(matrix.multiplied({.x = 1.0, .y = 0.0, .z = 0.0}), {.x = 0.0, .y = -1.0, .z = 0.0});
    compareVector(matrix.multiplied({.x = 0.0, .y = 1.0, .z = 0.0}), {.x = 1.0, .y = 0.0, .z = 0.0});
    compareVector(matrix.multiplied({.x = 0.0, .y = 0.0, .z = 1.0}), {.x = 0.0, .y = 0.0, .z = 1.0});
}

void Matrix3x3Tests::storesValuesInRowMajorOrder()
{
    const skygate::core::Matrix3x3 matrix{
        {.x = 1.0, .y = 2.0, .z = 3.0},
        {.x = 4.0, .y = 5.0, .z = 6.0},
        {.x = 7.0, .y = 8.0, .z = 9.0},
    };

    compareRow(matrix, 0U, {.x = 1.0, .y = 2.0, .z = 3.0});
    compareRow(matrix, 1U, {.x = 4.0, .y = 5.0, .z = 6.0});
    compareRow(matrix, 2U, {.x = 7.0, .y = 8.0, .z = 9.0});
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

void Matrix3x3Tests::transposeMultipliedMapsBasisVectorsToRows()
{
    const skygate::core::Matrix3x3 matrix{
        {.x = 1.0, .y = 2.0, .z = 3.0},
        {.x = 4.0, .y = 5.0, .z = 6.0},
        {.x = 7.0, .y = 8.0, .z = 9.0},
    };

    compareVector(matrix.transposeMultiplied({.x = 1.0, .y = 0.0, .z = 0.0}), {.x = 1.0, .y = 2.0, .z = 3.0});
    compareVector(matrix.transposeMultiplied({.x = 0.0, .y = 1.0, .z = 0.0}), {.x = 4.0, .y = 5.0, .z = 6.0});
    compareVector(matrix.transposeMultiplied({.x = 0.0, .y = 0.0, .z = 1.0}), {.x = 7.0, .y = 8.0, .z = 9.0});
}

QTEST_APPLESS_MAIN(Matrix3x3Tests)

#include "Matrix3x3Tests.moc"
