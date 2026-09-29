#include "EphemerisDataPayloadReader.hpp"

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QFile>
#include <QIODevice>
#include <QLibrary>

#include <array>
#include <cstddef>
#include <string_view>

namespace skygate::ephemeris {

namespace {

constexpr std::size_t kIoBufferBytes = 1U << 16U;

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

void addDiagnostic(EphemerisDataPayloadReader::Result& result, const std::string_view diagnostic)
{
    result.diagnostics.emplace_back(diagnostic);
}

[[nodiscard]] bool cancellationRequested(const std::function<bool()>& callback)
{
    return callback != nullptr && callback();
}

void markCanceled(EphemerisDataPayloadReader::Result& result)
{
    result.status = EphemerisDataPayloadReader::Status::Canceled;
    addDiagnostic(result, "Ephemeris data activation was canceled.");
}

[[nodiscard]] bool copyUncompressedAsset(
    QFile& sourceFile,
    QIODevice& targetFile,
    QCryptographicHash& hash,
    std::uint64_t& outputBytes,
    EphemerisDataPayloadReader::Result& result,
    const std::function<bool()>& cancellationCallback
)
{
    std::array<char, kIoBufferBytes> buffer{};
    while (!sourceFile.atEnd()) {
        if (cancellationRequested(cancellationCallback)) {
            markCanceled(result);
            return false;
        }
        const qint64 bytesRead = sourceFile.read(buffer.data(), static_cast<qint64>(buffer.size()));
        if (bytesRead < 0) {
            addDiagnostic(result, "Unable to read bundled ephemeris data asset.");
            return false;
        }
        if (bytesRead == 0) {
            continue;
        }
        if (targetFile.write(buffer.data(), bytesRead) != bytesRead) {
            addDiagnostic(result, "Unable to write ephemeris data asset into the writable cache.");
            return false;
        }
        hash.addData(QByteArrayView(buffer.data(), bytesRead));
        outputBytes += static_cast<std::uint64_t>(bytesRead);
    }

    return true;
}

[[nodiscard]] bool decompressZstdAsset(
    QFile& sourceFile,
    QIODevice& targetFile,
    QCryptographicHash& hash,
    std::uint64_t& outputBytes,
    EphemerisDataPayloadReader::Result& result,
    const std::function<bool()>& cancellationCallback
)
{
    const ZstdRuntime& runtime = ZstdRuntime::instance();
    if (!runtime.isAvailable()) {
        result.status = EphemerisDataPayloadReader::Status::UnsupportedCompression;
        addDiagnostic(result, "zstd runtime library is not available.");
        return false;
    }

    ScopedZstdDStream stream(runtime);
    if (stream.get() == nullptr || !runtime.initDStream(stream.get())) {
        result.status = EphemerisDataPayloadReader::Status::UnsupportedCompression;
        addDiagnostic(result, "Unable to initialize zstd decompression.");
        return false;
    }

    std::array<char, kIoBufferBytes> inputBuffer{};
    std::array<char, kIoBufferBytes> outputBuffer{};
    std::size_t remainingHint = 1U;
    bool sawFrameEnd = false;
    while (!sourceFile.atEnd()) {
        if (cancellationRequested(cancellationCallback)) {
            markCanceled(result);
            return false;
        }
        const qint64 bytesRead = sourceFile.read(inputBuffer.data(), static_cast<qint64>(inputBuffer.size()));
        if (bytesRead < 0) {
            addDiagnostic(result, "Unable to read compressed ephemeris data asset.");
            return false;
        }

        ZstdInBuffer input{inputBuffer.data(), static_cast<std::size_t>(bytesRead), 0U};
        while (input.pos < input.size) {
            if (cancellationRequested(cancellationCallback)) {
                markCanceled(result);
                return false;
            }
            ZstdOutBuffer output{outputBuffer.data(), outputBuffer.size(), 0U};
            remainingHint = runtime.decompressStream(stream.get(), output, input);
            if (runtime.isError(remainingHint)) {
                result.status = EphemerisDataPayloadReader::Status::CorruptArchive;
                addDiagnostic(result, "zstd ephemeris data asset is corrupt or unsupported.");
                return false;
            }
            if (output.pos > 0U) {
                if (targetFile.write(outputBuffer.data(), static_cast<qint64>(output.pos))
                    != static_cast<qint64>(output.pos)) {
                    addDiagnostic(result, "Unable to write decompressed ephemeris data into the writable cache.");
                    return false;
                }
                hash.addData(QByteArrayView(outputBuffer.data(), static_cast<qsizetype>(output.pos)));
                outputBytes += static_cast<std::uint64_t>(output.pos);
            }
            sawFrameEnd = remainingHint == 0U;
        }
    }

    if (!sawFrameEnd || remainingHint != 0U) {
        result.status = EphemerisDataPayloadReader::Status::CorruptArchive;
        addDiagnostic(result, "zstd ephemeris data asset ended before a complete frame was decoded.");
        return false;
    }

    return true;
}

}  // namespace

EphemerisDataPayloadReader::Result EphemerisDataPayloadReader::read(
    const EphemerisDataManifestCompressionKind compression,
    QFile& sourceFile,
    QIODevice& targetFile,
    const std::function<bool()>& cancellationCallback
)
{
    Result result;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    bool read = false;
    switch (compression) {
    case EphemerisDataManifestCompressionKind::None:
        read = copyUncompressedAsset(sourceFile, targetFile, hash, result.outputBytes, result, cancellationCallback);
        break;
    case EphemerisDataManifestCompressionKind::Zstd:
        read = decompressZstdAsset(sourceFile, targetFile, hash, result.outputBytes, result, cancellationCallback);
        break;
    }
    if (read) {
        result.status = Status::Read;
        result.checksum = hash.result().toHex().toStdString();
    }
    return result;
}

}  // namespace skygate::ephemeris
