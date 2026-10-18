#include "hash/Fnv1a64Hasher.hpp"
#include "hash/HashConstants.hpp"

#include <QtTest/QtTest>

#include <cstdint>
#include <string_view>

namespace {

constexpr std::uint64_t kEmptyHash = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t kLetterAHash = 12'638'187'200'555'641'996ULL;
constexpr std::uint64_t kFoobarHash = 9'625'390'261'332'436'968ULL;

}  // namespace

class Fnv1a64HasherTests final : public QObject {
    Q_OBJECT

private slots:
    void emptyInputReturnsOffsetBasis();
    void matchesKnownFnv1aVectors();
    void hashFromContinuesExistingState();
    void combineAndAppendComponentMixState();
};

void Fnv1a64HasherTests::emptyInputReturnsOffsetBasis()
{
    QCOMPARE(skygate::core::Fnv1a64Hasher::hash(std::string_view{}), kEmptyHash);
    QCOMPARE(skygate::core::HashConstants::kFnv1a64OffsetBasis, kEmptyHash);
}

void Fnv1a64HasherTests::matchesKnownFnv1aVectors()
{
    QCOMPARE(skygate::core::Fnv1a64Hasher::hash(std::string_view{"a"}), kLetterAHash);
    QCOMPARE(skygate::core::Fnv1a64Hasher::hash(std::string_view{"foobar"}), kFoobarHash);
}

void Fnv1a64HasherTests::hashFromContinuesExistingState()
{
    constexpr std::string_view value = "foobar";
    const std::uint64_t prefixHash =
        skygate::core::Fnv1a64Hasher::hashFrom(skygate::core::HashConstants::kFnv1a64OffsetBasis, value.substr(0, 3));
    const std::uint64_t continuedHash = skygate::core::Fnv1a64Hasher::hashFrom(prefixHash, value.substr(3));

    QCOMPARE(continuedHash, skygate::core::Fnv1a64Hasher::hash(value));
}

void Fnv1a64HasherTests::combineAndAppendComponentMixState()
{
    const std::uint64_t state = skygate::core::Fnv1a64Hasher::hash(std::string_view{"seed"});
    const std::uint64_t appended = skygate::core::Fnv1a64Hasher::appendComponent(state, std::string_view{"first"});
    const std::uint64_t combined = skygate::core::Fnv1a64Hasher::combine(appended, 7U);

    QVERIFY(combined != appended);
    QVERIFY(
        skygate::core::Fnv1a64Hasher::appendComponent(state, std::string_view{"ab"})
        != skygate::core::Fnv1a64Hasher::appendComponent(
            skygate::core::Fnv1a64Hasher::appendComponent(state, std::string_view{"a"}), std::string_view{"b"}
        )
    );
}

QTEST_APPLESS_MAIN(Fnv1a64HasherTests)

#include "Fnv1a64HasherTests.moc"
