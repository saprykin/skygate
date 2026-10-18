#include "engine/BitFlagSet.hpp"

#include <QtTest/QtTest>

#include <cstdint>

namespace {

// One flag type that stores bit positions and one that stores masks, so both
// ways of naming a flag travel through the same set type.
struct FlagSamples {
    enum class Position : std::uint8_t {
        First,
        Second,
        Third
    };

    enum class Mask : std::uint8_t {
        None = 0U,
        Alpha = 1U << 0U,
        Beta = 1U << 1U,
        Gamma = 1U << 2U,
        AlphaAndGamma = Alpha | Gamma
    };
};

using skygate::ephemeris::BitFlagSet;

using PositionSet = BitFlagSet<FlagSamples::Position>;
using MaskSet = BitFlagSet<FlagSamples::Mask>;

}  // namespace

class BitFlagSetTests final : public QObject {
    Q_OBJECT

private slots:
    void emptySetHoldsNoFlags();
    void bitMapsPositionToSingleBit();
    void addAccumulatesPositions();
    void maskEnumeratorConvertsDirectly();
    void hasAcceptsSingleBitAndMaskFlag();
    void mergeCombinesBothMasks();
    void withoutRemovesRequestedFlags();
    void operatorsCombineAndCompare();
};

void BitFlagSetTests::emptySetHoldsNoFlags()
{
    const PositionSet flags;

    QCOMPARE(flags.bits(), std::uint32_t{0});
    QVERIFY(!flags.hasAny());
    QVERIFY(!flags.has(PositionSet::bit(FlagSamples::Position::First)));
}

void BitFlagSetTests::bitMapsPositionToSingleBit()
{
    QCOMPARE(PositionSet::bit(FlagSamples::Position::First), std::uint32_t{1});
    QCOMPARE(PositionSet::bit(FlagSamples::Position::Second), std::uint32_t{2});
    QCOMPARE(PositionSet::bit(FlagSamples::Position::Third), std::uint32_t{4});
}

void BitFlagSetTests::addAccumulatesPositions()
{
    PositionSet flags;
    flags.add(PositionSet::bit(FlagSamples::Position::First));
    flags.add(PositionSet::bit(FlagSamples::Position::Third));

    QCOMPARE(flags.bits(), std::uint32_t{5});
    QVERIFY(flags.has(PositionSet::bit(FlagSamples::Position::First)));
    QVERIFY(flags.has(PositionSet::bit(FlagSamples::Position::Third)));
    QVERIFY(!flags.has(PositionSet::bit(FlagSamples::Position::Second)));

    flags.add(PositionSet::bit(FlagSamples::Position::Third));
    QCOMPARE(flags.bits(), std::uint32_t{5});
}

void BitFlagSetTests::maskEnumeratorConvertsDirectly()
{
    const MaskSet both = FlagSamples::Mask::AlphaAndGamma;

    QCOMPARE(both.bits(), std::uint32_t{5});
    QVERIFY(both.has(FlagSamples::Mask::Alpha));
    QVERIFY(both.has(FlagSamples::Mask::Gamma));
    QVERIFY(!both.has(FlagSamples::Mask::Beta));

    const MaskSet none = FlagSamples::Mask::None;
    QVERIFY(!none.hasAny());

    const MaskSet fromBits = MaskSet::fromBits(4U);
    QCOMPARE(fromBits.bits(), std::uint32_t{4});
    QVERIFY(fromBits.has(FlagSamples::Mask::Gamma));
}

void BitFlagSetTests::hasAcceptsSingleBitAndMaskFlag()
{
    PositionSet flags;
    flags.add(PositionSet::bit(FlagSamples::Position::Second));

    QVERIFY(flags.has(PositionSet::bit(FlagSamples::Position::Second)));
    QVERIFY(!flags.has(PositionSet::bit(FlagSamples::Position::First)));

    const MaskSet masks = FlagSamples::Mask::Alpha;
    QVERIFY(masks.has(FlagSamples::Mask::Alpha));
    QVERIFY(masks.has(FlagSamples::Mask::AlphaAndGamma));
    QVERIFY(!masks.has(FlagSamples::Mask::Beta));
}

void BitFlagSetTests::mergeCombinesBothMasks()
{
    PositionSet flags;
    flags.add(PositionSet::bit(FlagSamples::Position::First));
    PositionSet other;
    other.add(PositionSet::bit(FlagSamples::Position::Third));

    flags.merge(other);

    QCOMPARE(flags.bits(), std::uint32_t{5});
    QVERIFY(flags.has(PositionSet::bit(FlagSamples::Position::First)));
    QVERIFY(flags.has(PositionSet::bit(FlagSamples::Position::Third)));
}

void BitFlagSetTests::withoutRemovesRequestedFlags()
{
    const MaskSet flags = MaskSet::fromBits(7U);

    const MaskSet withoutAlpha = flags.without(FlagSamples::Mask::Alpha);
    QCOMPARE(withoutAlpha.bits(), std::uint32_t{6});
    QVERIFY(!withoutAlpha.has(FlagSamples::Mask::Alpha));
    QVERIFY(withoutAlpha.has(FlagSamples::Mask::Beta));

    const MaskSet unchanged = flags.without(FlagSamples::Mask::None);
    QCOMPARE(unchanged, flags);
    QCOMPARE(flags.bits(), std::uint32_t{7});
}

void BitFlagSetTests::operatorsCombineAndCompare()
{
    const PositionSet first = PositionSet::fromBits(PositionSet::bit(FlagSamples::Position::First));
    const PositionSet third = PositionSet::fromBits(PositionSet::bit(FlagSamples::Position::Third));

    const PositionSet combined = first | third;
    QCOMPARE(combined.bits(), std::uint32_t{5});
    QCOMPARE((combined & first).bits(), std::uint32_t{1});
    QVERIFY(!(combined & PositionSet::fromBits(PositionSet::bit(FlagSamples::Position::Second))).hasAny());

    PositionSet accumulated = first;
    accumulated |= third;
    QCOMPARE(accumulated, combined);
    QVERIFY(accumulated != first);
    QCOMPARE(static_cast<std::uint32_t>(accumulated), std::uint32_t{5});
}

QTEST_APPLESS_MAIN(BitFlagSetTests)

#include "BitFlagSetTests.moc"
