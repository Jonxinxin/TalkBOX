#pragma once

#include <QAtomicInt>
#include <QObject>
#include <QString>
#include <QStringList>

namespace talkbox {

/// Central attachment rules (F-09): allowed formats and size caps live
/// here so they can be adjusted in one place.
namespace attachmentrules {

constexpr qint64 kMaxImageBytes = 50LL * 1024 * 1024; // 50 MB
constexpr qint64 kMaxPdfBytes = 100LL * 1024 * 1024;  // 100 MB

QStringList imageExtensions();
QStringList allowedExtensions(const QString &type);
qint64 maxBytesForType(const QString &type);
QString mimeForExtension(const QString &extension);

} // namespace attachmentrules

/// Imports files into the managed attachments directory.
///
/// - Files are validated (existence, extension, size, basic format) on
///   the caller thread.
/// - The actual copy runs on a worker thread; results arrive through
///   importFinished().
/// - Stored file names are always <uuid>.<ext>; the user-facing display
///   name never influences the file on disk.
class AttachmentService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    /// dataRoot defaults to AppPaths::dataDir(); tests pass a temporary
    /// directory instead. Attachments live in <dataRoot>/attachments.
    explicit AttachmentService(QString dataRoot = QString(), QObject *parent = nullptr);

    QString lastError() const;
    bool busy() const;

    /// Validates the file and starts an asynchronous copy. Returns a
    /// token used to match importFinished(); an empty token means the
    /// request was rejected immediately (see lastError()).
    Q_INVOKABLE QString startImport(const QString &sourceUrl, const QString &type);

    /// Removes a managed file. Paths outside attachments/ are refused.
    /// A missing file counts as success.
    Q_INVOKABLE bool removeManagedFile(const QString &relativePath);

    /// Absolute path of a managed file (empty for invalid input).
    Q_INVOKABLE QString absolutePath(const QString &relativePath) const;

    Q_INVOKABLE bool managedFileExists(const QString &relativePath) const;

    /// Size of a local file in bytes, -1 when unavailable.
    Q_INVOKABLE qint64 fileSizeFor(const QString &sourceUrl) const;

    static QString attachmentsPrefix();

signals:
    void importFinished(const QString &token, bool ok, const QString &error,
                        const QString &relativePath, const QString &mimeType, qint64 fileSize);
    void lastErrorChanged();
    void busyChanged();

private:
    bool fail(const QString &message);
    void reportImportFinished(const QString &token, bool ok, const QString &error,
                              const QString &relativePath, const QString &mimeType,
                              qint64 fileSize);

    QString m_dataRoot;
    QString m_attachmentsDir;
    QString m_lastError;
    QAtomicInt m_busyCount{0};
};

} // namespace talkbox
