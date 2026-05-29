#include "math/Vector3d.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <limits>

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

class Vector3dTests final : public QObject {
    Q_OBJECT

private slots:
    void reportsFiniteComponents();
    void computesLengthAndNormalization();
    void rejectsZeroAndNonFiniteNormalization();
    void computesDotAndCrossProducts();
    void supportsArithmeticOperators();
};

void Vector3dTests::reportsFiniteComponents()
{
    const skygate::core::Vector3d finiteVector{.x = 1.0, .y = -2.0, .z = 3.0};
    const skygate::core::Vector3d nonFiniteVector{
        .x = std::numeric_limits<double>::quiet_NaN(),
        .y = 0.0,
        .z = 0.0,
    };
    QVERIFY(finiteVector.isFinite());
    QVERIFY(!nonFiniteVector.isFinite());
}

void Vector3dTests::computesLengthAndNormalization()
{
    const skygate::core::Vector3d vector{.x = 2.0, .y = 3.0, .z = 6.0};
    QVERIFY(isNear(vector.length(), 7.0));

    const std::optional<skygate::core::Vector3d> normalized = vector.normalized();
    QVERIFY(normalized.has_value());
    compareVector(*normalized, {.x = 2.0 / 7.0, .y = 3.0 / 7.0, .z = 6.0 / 7.0});
}

void Vector3dTests::rejectsZeroAndNonFiniteNormalization()
{
    const skygate::core::Vector3d nonFiniteVector{
        .x = 0.0,
        .y = std::numeric_limits<double>::infinity(),
        .z = 0.0,
    };

    QVERIFY(!skygate::core::Vector3d{}.normalized().has_value());
    QVERIFY(!nonFiniteVector.normalized().has_value());
}

void Vector3dTests::computesDotAndCrossProducts()
{
    const skygate::core::Vector3d xAxis{.x = 1.0, .y = 0.0, .z = 0.0};
    const skygate::core::Vector3d yAxis{.x = 0.0, .y = 1.0, .z = 0.0};
    const skygate::core::Vector3d zAxis{.x = 0.0, .y = 0.0, .z = 1.0};

    QCOMPARE(xAxis.dot(yAxis), 0.0);
    QCOMPARE(xAxis.dot(xAxis), 1.0);
    compareVector(xAxis.cross(yAxis), zAxis);
    compareVector(yAxis.cross(xAxis), {.x = 0.0, .y = 0.0, .z = -1.0});
}

void Vector3dTests::supportsArithmeticOperators()
{
    skygate::core::Vector3d vector{.x = 1.0, .y = 2.0, .z = 3.0};

    compareVector(vector + skygate::core::Vector3d{.x = 4.0, .y = -1.0, .z = 2.0}, {.x = 5.0, .y = 1.0, .z = 5.0});
    compareVector(vector - skygate::core::Vector3d{.x = 4.0, .y = -1.0, .z = 2.0}, {.x = -3.0, .y = 3.0, .z = 1.0});
    compareVector(vector * 2.0, {.x = 2.0, .y = 4.0, .z = 6.0});
    compareVector(vector.scaled(-1.0), {.x = -1.0, .y = -2.0, .z = -3.0});

    vector += {.x = 1.0, .y = 1.0, .z = 1.0};
    compareVector(vector, {.x = 2.0, .y = 3.0, .z = 4.0});
    vector -= {.x = 1.0, .y = 2.0, .z = 3.0};
    compareVector(vector, {.x = 1.0, .y = 1.0, .z = 1.0});
    vector *= 3.0;
    compareVector(vector, {.x = 3.0, .y = 3.0, .z = 3.0});
}

QTEST_APPLESS_MAIN(Vector3dTests)

#include "Vector3dTests.moc"
