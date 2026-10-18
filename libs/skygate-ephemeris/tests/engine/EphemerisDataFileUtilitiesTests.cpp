#include "engine/EphemerisDataActivationStatus.hpp"
#include "engine/EphemerisDataFileUtilities.hpp"
#include "engine/EphemerisDataPayloadReader.hpp"
#include "engine/EphemerisStagedUpdateVerificationStatus.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

class EphemerisDataFileUtilitiesTests final : public QObject {
    Q_OBJECT

private slots:
    void rejectsUnsafePathComponents();
    void convertsPathsToQString();
    void comparesChecksumsIgnoringAsciiCase();
    void matchesSourceSizeAgainstMetadata();
    void reportsCancellationRequests();
    void mapsPayloadStatusToActivationStatus();
    void mapsPayloadStatusToVerificationStatus();
    void verifiesDecodedPayloadAgainstMetadata();
};

void EphemerisDataFileUtilitiesTests::rejectsUnsafePathComponents()
{
    using skygate::ephemeris::EphemerisDataFileUtilities;

    QVERIFY(EphemerisDataFileUtilities::hasUnsafePathComponent(std::filesystem::path{}));
    QVERIFY(EphemerisDataFileUtilities::hasUnsafePathComponent(std::filesystem::path("/absolute/file.bin")));
    QVERIFY(EphemerisDataFileUtilities::hasUnsafePathComponent(std::filesystem::path("../escape.bin")));
    QVERIFY(EphemerisDataFileUtilities::hasUnsafePathComponent(std::filesystem::path("nested/../escape.bin")));
    QVERIFY(EphemerisDataFileUtilities::hasUnsafePathComponent(std::filesystem::path("nested/./file.bin")));

    QVERIFY(!EphemerisDataFileUtilities::hasUnsafePathComponent(std::filesystem::path("profiles/modern/kernel.bsp")));
    QVERIFY(!EphemerisDataFileUtilities::hasUnsafePathComponent(std::filesystem::path("leap-seconds.txt")));
}

void EphemerisDataFileUtilitiesTests::convertsPathsToQString()
{
    using skygate::ephemeris::EphemerisDataFileUtilities;

    QCOMPARE(
        EphemerisDataFileUtilities::pathToQString(std::filesystem::path("profiles/modern/kernel.bsp")),
        QStringLiteral("profiles/modern/kernel.bsp")
    );
    QCOMPARE(EphemerisDataFileUtilities::pathToQString(std::filesystem::path{}), QString());
}

void EphemerisDataFileUtilitiesTests::comparesChecksumsIgnoringAsciiCase()
{
    using skygate::ephemeris::EphemerisDataFileUtilities;

    QVERIFY(EphemerisDataFileUtilities::checksumsMatch("0123abcd", "0123ABCD"));
    QVERIFY(!EphemerisDataFileUtilities::checksumsMatch("0123abcd", "0123abce"));
    QVERIFY(!EphemerisDataFileUtilities::checksumsMatch("0123abcd", "0123abc"));
    QVERIFY(EphemerisDataFileUtilities::checksumsMatch("", ""));
}

void EphemerisDataFileUtilitiesTests::matchesSourceSizeAgainstMetadata()
{
    using skygate::ephemeris::EphemerisDataFileUtilities;
    using skygate::ephemeris::EphemerisDataManifest;

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = directory.filePath(QStringLiteral("payload.bin"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("data", 4), 4LL);
    file.close();

    const QFileInfo sourceInfo(filePath);
    EphemerisDataManifest::Asset asset;
    QVERIFY(EphemerisDataFileUtilities::sourceSizeMatchesMetadata(sourceInfo, asset));

    asset.compression.compressedSizeBytes = 4U;
    QVERIFY(EphemerisDataFileUtilities::sourceSizeMatchesMetadata(sourceInfo, asset));

    asset.compression.compressedSizeBytes = 5U;
    QVERIFY(!EphemerisDataFileUtilities::sourceSizeMatchesMetadata(sourceInfo, asset));
}

void EphemerisDataFileUtilitiesTests::reportsCancellationRequests()
{
    using skygate::ephemeris::EphemerisDataFileUtilities;

    QVERIFY(!EphemerisDataFileUtilities::cancellationRequested(nullptr));
    QVERIFY(!EphemerisDataFileUtilities::cancellationRequested(std::function<bool()>{}));
    QVERIFY(!EphemerisDataFileUtilities::cancellationRequested([] { return false; }));
    QVERIFY(EphemerisDataFileUtilities::cancellationRequested([] { return true; }));
}

void EphemerisDataFileUtilitiesTests::mapsPayloadStatusToActivationStatus()
{
    using skygate::ephemeris::EphemerisDataActivationStatus;
    using skygate::ephemeris::EphemerisDataFileUtilities;
    using skygate::ephemeris::EphemerisDataPayloadReader;

    QCOMPARE(
        EphemerisDataFileUtilities::payloadStatus<EphemerisDataActivationStatus>(
            EphemerisDataPayloadReader::Status::UnsupportedCompression
        ),
        EphemerisDataActivationStatus::UnsupportedCompression
    );
    QCOMPARE(
        EphemerisDataFileUtilities::payloadStatus<EphemerisDataActivationStatus>(
            EphemerisDataPayloadReader::Status::CorruptArchive
        ),
        EphemerisDataActivationStatus::CorruptArchive
    );
    QCOMPARE(
        EphemerisDataFileUtilities::payloadStatus<EphemerisDataActivationStatus>(
            EphemerisDataPayloadReader::Status::Canceled
        ),
        EphemerisDataActivationStatus::Canceled
    );
    QCOMPARE(
        EphemerisDataFileUtilities::payloadStatus<EphemerisDataActivationStatus>(
            EphemerisDataPayloadReader::Status::IoError
        ),
        EphemerisDataActivationStatus::IoError
    );
    QCOMPARE(
        EphemerisDataFileUtilities::payloadStatus<EphemerisDataActivationStatus>(
            EphemerisDataPayloadReader::Status::OutputLimitExceeded
        ),
        EphemerisDataActivationStatus::ChecksumMismatch
    );
    QCOMPARE(
        EphemerisDataFileUtilities::payloadStatus<EphemerisDataActivationStatus>(
            EphemerisDataPayloadReader::Status::Read
        ),
        EphemerisDataActivationStatus::IoError
    );
}

void EphemerisDataFileUtilitiesTests::mapsPayloadStatusToVerificationStatus()
{
    using skygate::ephemeris::EphemerisDataFileUtilities;
    using skygate::ephemeris::EphemerisDataPayloadReader;
    using skygate::ephemeris::EphemerisStagedUpdateVerificationStatus;

    QCOMPARE(
        EphemerisDataFileUtilities::payloadStatus<EphemerisStagedUpdateVerificationStatus>(
            EphemerisDataPayloadReader::Status::UnsupportedCompression
        ),
        EphemerisStagedUpdateVerificationStatus::UnsupportedCompression
    );
    QCOMPARE(
        EphemerisDataFileUtilities::payloadStatus<EphemerisStagedUpdateVerificationStatus>(
            EphemerisDataPayloadReader::Status::CorruptArchive
        ),
        EphemerisStagedUpdateVerificationStatus::CorruptArchive
    );
    QCOMPARE(
        EphemerisDataFileUtilities::payloadStatus<EphemerisStagedUpdateVerificationStatus>(
            EphemerisDataPayloadReader::Status::Canceled
        ),
        EphemerisStagedUpdateVerificationStatus::Canceled
    );
    QCOMPARE(
        EphemerisDataFileUtilities::payloadStatus<EphemerisStagedUpdateVerificationStatus>(
            EphemerisDataPayloadReader::Status::IoError
        ),
        EphemerisStagedUpdateVerificationStatus::IoError
    );
    QCOMPARE(
        EphemerisDataFileUtilities::payloadStatus<EphemerisStagedUpdateVerificationStatus>(
            EphemerisDataPayloadReader::Status::OutputLimitExceeded
        ),
        EphemerisStagedUpdateVerificationStatus::ChecksumMismatch
    );
    QCOMPARE(
        EphemerisDataFileUtilities::payloadStatus<EphemerisStagedUpdateVerificationStatus>(
            EphemerisDataPayloadReader::Status::Read
        ),
        EphemerisStagedUpdateVerificationStatus::IoError
    );
}

void EphemerisDataFileUtilitiesTests::verifiesDecodedPayloadAgainstMetadata()
{
    using skygate::ephemeris::EphemerisDataFileUtilities;
    using skygate::ephemeris::EphemerisDataManifest;
    using PayloadVerification = skygate::ephemeris::EphemerisDataFileUtilities::PayloadVerification;

    EphemerisDataManifest::Asset asset;
    asset.checksum.algorithm = std::string{EphemerisDataFileUtilities::kSha256Algorithm};
    asset.checksum.value = "0123abcd";
    asset.compression.uncompressedSizeBytes = 4U;

    QCOMPARE(
        static_cast<std::uint8_t>(EphemerisDataFileUtilities::verifyDecodedPayload(asset, 4U, "0123ABCD")),
        static_cast<std::uint8_t>(PayloadVerification::Verified)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(EphemerisDataFileUtilities::verifyDecodedPayload(asset, 4U, "0123abce")),
        static_cast<std::uint8_t>(PayloadVerification::ChecksumMismatch)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(EphemerisDataFileUtilities::verifyDecodedPayload(asset, 5U, "0123abcd")),
        static_cast<std::uint8_t>(PayloadVerification::UncompressedSizeMismatch)
    );

    EphemerisDataManifest::Asset unpinnedSizeAsset = asset;
    unpinnedSizeAsset.compression.uncompressedSizeBytes.reset();
    QCOMPARE(
        static_cast<std::uint8_t>(EphemerisDataFileUtilities::verifyDecodedPayload(unpinnedSizeAsset, 5U, "0123abcd")),
        static_cast<std::uint8_t>(PayloadVerification::Verified)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(EphemerisDataFileUtilities::verifyDecodedPayload(unpinnedSizeAsset, 5U, "ffffffff")),
        static_cast<std::uint8_t>(PayloadVerification::ChecksumMismatch)
    );

    EphemerisDataManifest::Asset liveAsset = asset;
    liveAsset.live = true;
    QCOMPARE(
        static_cast<std::uint8_t>(EphemerisDataFileUtilities::verifyDecodedPayload(liveAsset, 4U, "ffffffff")),
        static_cast<std::uint8_t>(PayloadVerification::Verified)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(EphemerisDataFileUtilities::verifyDecodedPayload(liveAsset, 0U, "0123abcd")),
        static_cast<std::uint8_t>(PayloadVerification::EmptyLivePayload)
    );
}

QTEST_APPLESS_MAIN(EphemerisDataFileUtilitiesTests)

#include "EphemerisDataFileUtilitiesTests.moc"
