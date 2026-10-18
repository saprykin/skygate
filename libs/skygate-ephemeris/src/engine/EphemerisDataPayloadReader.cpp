#include "EphemerisDataPayloadReader.hpp"
#include "EphemerisPayloadDiagnostics.hpp"

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QFile>
#include <QIODevice>
#include <QLibrary>

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace skygate::ephemeris {

namespace {

using PayloadDiagnostics = EphemerisPayloadDiagnostics;

constexpr std::string_view kCanceledDiagnostic = "Ephemeris data payload read was canceled.";

struct ZstdInBuffer {
    const void* src = nullptr;
    std::size_t size = 0U;
    std::size_t pos = 0U;
};

struct ZstdOutBuffer {
    void* dst = nullptr;
    std::size_t size = 0U;
    std::size_t pos = 0U;
};

struct ZstdDStream;

class ZstdRuntime final {
public:
    using CreateDStream = ZstdDStream* (*)();
    using FreeDStream = std::size_t (*)(ZstdDStream*);
    using InitDStream = std::size_t (*)(ZstdDStream*);
    using DecompressStream = std::size_t (*)(ZstdDStream*, ZstdOutBuffer*, ZstdInBuffer*);
    using IsError = unsigned int (*)(std::size_t);

    [[nodiscard]] static const ZstdRuntime& instance()
    {
        static const ZstdRuntime runtime;
        return runtime;
    }

    [[nodiscard]] bool isAvailable() const noexcept
    {
        return m_available;
    }

    [[nodiscard]] ZstdDStream* createDStream() const
    {
        return m_createDStream == nullptr ? nullptr : m_createDStream();
    }

    void freeDStream(ZstdDStream* stream) const
    {
        if (stream != nullptr && m_freeDStream != nullptr) {
            (void)m_freeDStream(stream);
        }
    }

    [[nodiscard]] bool initDStream(ZstdDStream* stream) const
    {
        return m_initDStream != nullptr && !isError(m_initDStream(stream));
    }

    [[nodiscard]] std::size_t decompressStream(ZstdDStream* stream, ZstdOutBuffer& output, ZstdInBuffer& input) const
    {
        return m_decompressStream(stream, &output, &input);
    }

    [[nodiscard]] bool isError(const std::size_t code) const
    {
        return m_isError == nullptr || m_isError(code) != 0U;
    }

private:
    ZstdRuntime()
    {
        for (const QString& libraryName :
             {QStringLiteral("zstd"), QStringLiteral("libzstd"), QStringLiteral("libzstd.so.1")}) {
            m_library.setFileName(libraryName);
            if (!m_library.load()) {
                continue;
            }

            m_createDStream = reinterpret_cast<CreateDStream>(m_library.resolve("ZSTD_createDStream"));
            m_freeDStream = reinterpret_cast<FreeDStream>(m_library.resolve("ZSTD_freeDStream"));
            m_initDStream = reinterpret_cast<InitDStream>(m_library.resolve("ZSTD_initDStream"));
            m_decompressStream = reinterpret_cast<DecompressStream>(m_library.resolve("ZSTD_decompressStream"));
            m_isError = reinterpret_cast<IsError>(m_library.resolve("ZSTD_isError"));
            m_available = m_createDStream != nullptr && m_freeDStream != nullptr && m_initDStream != nullptr
                          && m_decompressStream != nullptr && m_isError != nullptr;
            if (m_available) {
                return;
            }

            m_library.unload();
        }
    }

    mutable QLibrary m_library;
    CreateDStream m_createDStream = nullptr;
    FreeDStream m_freeDStream = nullptr;
    InitDStream m_initDStream = nullptr;
    DecompressStream m_decompressStream = nullptr;
    IsError m_isError = nullptr;
    bool m_available = false;
};

class ScopedZstdDStream final {
public:
    explicit ScopedZstdDStream(const ZstdRuntime& runtime) : m_runtime(runtime), m_stream(runtime.createDStream()) {}

    ~ScopedZstdDStream()
    {
        m_runtime.freeDStream(m_stream);
    }

    ScopedZstdDStream(const ScopedZstdDStream&) = delete;
    ScopedZstdDStream& operator=(const ScopedZstdDStream&) = delete;

    [[nodiscard]] ZstdDStream* get() const noexcept
    {
        return m_stream;
    }

private:
    const ZstdRuntime& m_runtime;
    ZstdDStream* m_stream = nullptr;
};

[[nodiscard]] bool copyUncompressedAsset(
    QFile& sourceFile,
    QIODevice& targetFile,
    QCryptographicHash& hash,
    std::uint64_t& outputBytes,
    const std::optional<std::uint64_t>& maximumOutputBytes,
    EphemerisDataPayloadReader::Result& result,
    const std::function<bool()>& cancellationCallback
)
{
    std::array<char, EphemerisDataPayloadReader::kIoBufferBytes> buffer{};
    while (!sourceFile.atEnd()) {
        if (EphemerisDataPayloadReader::cancellationRequested(cancellationCallback)) {
            PayloadDiagnostics::markCanceled(result, kCanceledDiagnostic);
            return false;
        }
        const qint64 bytesRead = sourceFile.read(buffer.data(), static_cast<qint64>(buffer.size()));
        if (bytesRead < 0) {
            PayloadDiagnostics::addDiagnostic(result, "Unable to read bundled ephemeris data asset.");
            return false;
        }
        if (bytesRead == 0) {
            continue;
        }
        const std::uint64_t chunkBytes = static_cast<std::uint64_t>(bytesRead);
        if (maximumOutputBytes.has_value() && chunkBytes > *maximumOutputBytes - outputBytes) {
            result.status = EphemerisDataPayloadReader::Status::OutputLimitExceeded;
            PayloadDiagnostics::addDiagnostic(
                result, "Ephemeris data asset exceeds the configured maximum output size."
            );
            return false;
        }
        if (targetFile.write(buffer.data(), bytesRead) != bytesRead) {
            PayloadDiagnostics::addDiagnostic(result, "Unable to write ephemeris data asset into the writable cache.");
            return false;
        }
        hash.addData(QByteArrayView(buffer.data(), bytesRead));
        outputBytes += chunkBytes;
    }

    return true;
}

[[nodiscard]] bool decompressZstdAsset(
    QFile& sourceFile,
    QIODevice& targetFile,
    QCryptographicHash& hash,
    std::uint64_t& outputBytes,
    const std::optional<std::uint64_t>& maximumOutputBytes,
    EphemerisDataPayloadReader::Result& result,
    const std::function<bool()>& cancellationCallback
)
{
    const ZstdRuntime& runtime = ZstdRuntime::instance();
    if (!runtime.isAvailable()) {
        result.status = EphemerisDataPayloadReader::Status::UnsupportedCompression;
        PayloadDiagnostics::addDiagnostic(result, "zstd runtime library is not available.");
        return false;
    }

    ScopedZstdDStream stream(runtime);
    if (stream.get() == nullptr || !runtime.initDStream(stream.get())) {
        result.status = EphemerisDataPayloadReader::Status::UnsupportedCompression;
        PayloadDiagnostics::addDiagnostic(result, "Unable to initialize zstd decompression.");
        return false;
    }

    std::array<char, EphemerisDataPayloadReader::kIoBufferBytes> inputBuffer{};
    std::array<char, EphemerisDataPayloadReader::kIoBufferBytes> outputBuffer{};
    std::size_t remainingHint = 1U;
    bool sawFrameEnd = false;
    while (!sourceFile.atEnd()) {
        if (EphemerisDataPayloadReader::cancellationRequested(cancellationCallback)) {
            PayloadDiagnostics::markCanceled(result, kCanceledDiagnostic);
            return false;
        }
        const qint64 bytesRead = sourceFile.read(inputBuffer.data(), static_cast<qint64>(inputBuffer.size()));
        if (bytesRead < 0) {
            PayloadDiagnostics::addDiagnostic(result, "Unable to read compressed ephemeris data asset.");
            return false;
        }

        ZstdInBuffer input{inputBuffer.data(), static_cast<std::size_t>(bytesRead), 0U};
        while (input.pos < input.size) {
            if (EphemerisDataPayloadReader::cancellationRequested(cancellationCallback)) {
                PayloadDiagnostics::markCanceled(result, kCanceledDiagnostic);
                return false;
            }
            ZstdOutBuffer output{outputBuffer.data(), outputBuffer.size(), 0U};
            remainingHint = runtime.decompressStream(stream.get(), output, input);
            if (runtime.isError(remainingHint)) {
                result.status = EphemerisDataPayloadReader::Status::CorruptArchive;
                PayloadDiagnostics::addDiagnostic(result, "zstd ephemeris data asset is corrupt or unsupported.");
                return false;
            }
            if (output.pos > 0U) {
                const std::uint64_t chunkBytes = static_cast<std::uint64_t>(output.pos);
                if (maximumOutputBytes.has_value() && chunkBytes > *maximumOutputBytes - outputBytes) {
                    result.status = EphemerisDataPayloadReader::Status::OutputLimitExceeded;
                    PayloadDiagnostics::addDiagnostic(
                        result, "Ephemeris data asset exceeds the configured maximum output size."
                    );
                    return false;
                }
                if (targetFile.write(outputBuffer.data(), static_cast<qint64>(output.pos))
                    != static_cast<qint64>(output.pos)) {
                    PayloadDiagnostics::addDiagnostic(
                        result, "Unable to write decompressed ephemeris data into the writable cache."
                    );
                    return false;
                }
                hash.addData(QByteArrayView(outputBuffer.data(), static_cast<qsizetype>(output.pos)));
                outputBytes += chunkBytes;
            }
            sawFrameEnd = remainingHint == 0U;
        }
    }

    if (!sawFrameEnd || remainingHint != 0U) {
        result.status = EphemerisDataPayloadReader::Status::CorruptArchive;
        PayloadDiagnostics::addDiagnostic(
            result, "zstd ephemeris data asset ended before a complete frame was decoded."
        );
        return false;
    }

    return true;
}

}  // namespace

bool EphemerisDataPayloadReader::cancellationRequested(const std::function<bool()>& callback)
{
    return callback != nullptr && callback();
}

EphemerisDataPayloadReader::Result EphemerisDataPayloadReader::read(
    const EphemerisDataManifest::CompressionKind compression,
    QFile& sourceFile,
    QIODevice& targetFile,
    const std::function<bool()>& cancellationCallback,
    const std::optional<std::uint64_t> maximumOutputBytes
)
{
    Result result;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    bool read = false;
    switch (compression) {
    case EphemerisDataManifest::CompressionKind::None:
        read = copyUncompressedAsset(
            sourceFile, targetFile, hash, result.outputBytes, maximumOutputBytes, result, cancellationCallback
        );
        break;
    case EphemerisDataManifest::CompressionKind::Zstd:
        read = decompressZstdAsset(
            sourceFile, targetFile, hash, result.outputBytes, maximumOutputBytes, result, cancellationCallback
        );
        break;
    }
    if (read) {
        result.status = Status::Read;
        result.checksum = hash.result().toHex().toStdString();
    }
    return result;
}

}  // namespace skygate::ephemeris
