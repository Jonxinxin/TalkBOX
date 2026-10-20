#include "services/AttachmentService.h"

#include "app/AppPaths.h"

#include <QAtomicInt>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QPointer>
#include <QUrl>
#include <QUuid>

#include <QtConcurrent/QtConcurrentRun>

namespace talkbox {

namespace {

const QString kAttachmentsPrefix = QStringLiteral("attachments/");

QString displaySizeLimit(qint64 bytes)
{
    return QStringLiteral("%1 MB").arg(bytes / (1024 * 1024));
}

bool hasPdfMagic(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return file.read(5) == QByteArrayLiteral("%PDF-");
}

bool isReadableImage(const QString &path)
{
    QImageReader reader(path);
    return reader.canRead();
}

QString localPathFromUrlOrPath(const QString &source)
{
    const QUrl url(source);
    if (url.isLocalFile())
        return url.toLocalFile();
    return source;
}

} // namespace

namespace attachmentrules {

QStringList imageExtensions()
{
    return {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
            QStringLiteral("webp"), QStringLiteral("bmp")};
}

QStringList allowedExtensions(const QString &type)
{
    if (type == QLatin1String("image"))
        return imageExtensions();
    if (type == QLatin1String("pdf"))
        return {QStringLiteral("pdf")};
    return {};
}

qint64 maxBytesForType(const QString &type)
{
    if (type == QLatin1String("pdf"))
        return kMaxPdfBytes;
    return kMaxImageBytes;
}

QString mimeForExtension(const QString &extension)
{
    const QString ext = extension.toLower();
    if (ext == QLatin1String("png"))
        return QStringLiteral("image/png");
    if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg"))
        return QStringLiteral("image/jpeg");
    if (ext == QLatin1String("webp"))
        return QStringLiteral("image/webp");
    if (ext == QLatin1String("bmp"))
        return QStringLiteral("image/bmp");
    if (ext == QLatin1String("pdf"))
        return QStringLiteral("application/pdf");
    return QStringLiteral("application/octet-stream");
}

} // namespace attachmentrules

AttachmentService::AttachmentService(QString dataRoot, QObject *parent)
    : QObject(parent)
    , m_dataRoot(dataRoot.isEmpty() ? AppPaths::dataDir() : std::move(dataRoot))
    , m_attachmentsDir(m_dataRoot + QStringLiteral("/attachments"))
{
}

QString AttachmentService::lastError() const
{
    return m_lastError;
}

bool AttachmentService::busy() const
{
    return m_busyCount.loadRelaxed() > 0;
}

QString AttachmentService::attachmentsPrefix()
{
    return kAttachmentsPrefix;
}

QString AttachmentService::startImport(const QString &sourceUrl, const QString &type)
{
    if (type != QLatin1String("image") && type != QLatin1String("pdf")) {
        fail(tr("未知的内容类型。"));
        return {};
    }

    const QString sourcePath = localPathFromUrlOrPath(sourceUrl.trimmed());
    const QFileInfo info(sourcePath);
    if (!info.exists() || !info.isFile()) {
        fail(tr("所选文件不存在或不可读。"));
        return {};
    }

    const QString extension = info.suffix().toLower();
    if (!attachmentrules::allowedExtensions(type).contains(extension)) {
        fail(tr("不支持的文件格式：%1。图片支持 PNG/JPG/JPEG/WEBP/BMP，PDF 仅支持 .pdf。")
                 .arg(extension.isEmpty() ? tr("未知") : extension));
        return {};
    }

    const qint64 maxBytes = attachmentrules::maxBytesForType(type);
    if (info.size() > maxBytes) {
        fail(tr("文件超过大小限制（上限 %1，当前 %2 MB）。")
                 .arg(displaySizeLimit(maxBytes))
                 .arg(QString::number(double(info.size()) / (1024 * 1024), 'f', 1)));
        return {};
    }
    if (info.size() <= 0) {
        fail(tr("文件为空，无法导入。"));
        return {};
    }

    if (type == QLatin1String("pdf")) {
        if (!hasPdfMagic(sourcePath)) {
            fail(tr("该文件不是有效的 PDF 文档。"));
            return {};
        }
    } else {
        if (!isReadableImage(sourcePath)) {
            fail(tr("无法识别的图片文件，请确认格式未损坏。"));
            return {};
        }
    }

    const QString storedName =
        QUuid::createUuid().toString(QUuid::WithoutBraces) + QLatin1Char('.') + extension;
    const QString relativePath = kAttachmentsPrefix + storedName;
    const QString destinationPath = m_attachmentsDir + QLatin1Char('/') + storedName;
    const QString mimeType = attachmentrules::mimeForExtension(extension);
    const qint64 fileSize = info.size();
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);

    if (!QDir().mkpath(m_attachmentsDir)) {
        fail(tr("无法创建附件目录：%1").arg(QDir::toNativeSeparators(m_attachmentsDir)));
        return {};
    }

    m_busyCount.fetchAndAddRelaxed(1);
    emit busyChanged();

    QPointer<AttachmentService> guard(this);
    // Fire-and-forget: results are delivered through importFinished().
    (void)QtConcurrent::run([guard, token, sourcePath, destinationPath, relativePath, mimeType,
                       fileSize]() {
        QString error;
        bool ok = false;
        {
            QFile source(sourcePath);
            QFile destination(destinationPath);
            if (!source.open(QIODevice::ReadOnly)) {
                error = tr("无法读取源文件。");
            } else if (!destination.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
                error = tr("无法创建附件副本：%1").arg(destination.errorString());
            } else {
                ok = true;
                constexpr qint64 kChunkSize = 1024 * 1024;
                QByteArray buffer;
                buffer.resize(int(kChunkSize));
                qint64 remaining = fileSize;
                while (remaining > 0) {
                    const qint64 want = qMin(remaining, kChunkSize);
                    const qint64 got = source.read(buffer.data(), want);
                    if (got <= 0) {
                        ok = false;
                        error = tr("复制过程中断：%1").arg(source.errorString());
                        break;
                    }
                    if (destination.write(buffer.constData(), got) != got) {
                        ok = false;
                        error = tr("写入附件副本失败：%1").arg(destination.errorString());
                        break;
                    }
                    remaining -= got;
                }
                destination.close();
                source.close();
                if (!ok)
                    QFile::remove(destinationPath); // never leave partial copies behind
            }
        }

        if (!guard)
            return;
        // All state changes happen on the service's own thread.
        QMetaObject::invokeMethod(
            guard,
            [guard, token, ok, error, relativePath, mimeType, fileSize]() {
                if (!guard)
                    return;
                guard->m_busyCount.fetchAndSubRelaxed(1);
                emit guard->busyChanged();
                guard->reportImportFinished(token, ok, error, ok ? relativePath : QString(),
                                            ok ? mimeType : QString(), ok ? fileSize : -1);
            },
            Qt::QueuedConnection);
    });

    return token;
}

bool AttachmentService::removeManagedFile(const QString &relativePath)
{
    const QString normalized = QDir::cleanPath(relativePath);
    if (!normalized.startsWith(kAttachmentsPrefix) || normalized.contains(QLatin1String("..")))
        return fail(tr("非法的附件路径。"));

    QFile file(m_dataRoot + QLatin1Char('/') + normalized);
    if (!file.exists())
        return true; // already gone - deleting is idempotent
    if (!file.remove())
        return fail(tr("无法删除附件文件：%1").arg(file.errorString()));
    return true;
}

QString AttachmentService::absolutePath(const QString &relativePath) const
{
    const QString normalized = QDir::cleanPath(relativePath);
    if (!normalized.startsWith(kAttachmentsPrefix) || normalized.contains(QLatin1String("..")))
        return {};
    return m_dataRoot + QLatin1Char('/') + normalized;
}

bool AttachmentService::managedFileExists(const QString &relativePath) const
{
    const QString absolute = absolutePath(relativePath);
    return !absolute.isEmpty() && QFileInfo::exists(absolute);
}

qint64 AttachmentService::fileSizeFor(const QString &sourceUrl) const
{
    const QFileInfo info(localPathFromUrlOrPath(sourceUrl.trimmed()));
    return info.exists() && info.isFile() ? info.size() : -1;
}

bool AttachmentService::fail(const QString &message)
{
    if (m_lastError != message) {
        m_lastError = message;
        emit lastErrorChanged();
    }
    return false;
}

void AttachmentService::reportImportFinished(const QString &token, bool ok,
                                             const QString &errorMessage,
                                             const QString &relativePath,
                                             const QString &mimeType, qint64 fileSize)
{
    if (ok) {
        qInfo().noquote() << "Attachment imported:" << relativePath << fileSize << "bytes";
    } else {
        qWarning().noquote() << "Attachment import failed:" << errorMessage;
        fail(errorMessage);
    }
    emit importFinished(token, ok, errorMessage, relativePath, mimeType, fileSize);
}

} // namespace talkbox
