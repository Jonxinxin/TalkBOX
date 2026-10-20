#pragma once

#include <QObject>
#include <QString>

namespace talkbox {

class DatabaseManager;

/// Single-file backup and restore for the complete data set (PRD F-11).
///
/// The `.talkboxbackup` container is a ZIP archive holding:
///   manifest.json   format version, app version, created time, counts,
///                   attachment list with sizes
///   talkbox.db      consistent snapshot (VACUUM INTO, not a raw copy)
///   attachments/**  all managed files
///
/// Restore validates the container (type, manifest, version, every entry
/// path), extracts into a staging directory, verifies the staged database
/// (version + integrity + counts), preserves the current data in
/// `backups/pre-restore-<timestamp>/` and only then swaps the data in.
/// A failed restore always leaves the previous data usable.
class BackupService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    static constexpr int kFormatVersion = 1;

    static QString backupExtension();
    static QString manifestEntry();
    static QString databaseEntry();
    static QString attachmentsEntryRoot();

    /// Rejects absolute paths, drive letters and ".." traversal.
    static bool isSafeEntryPath(const QString &entryPath);
    /// Safe AND inside the expected container layout.
    static bool isAllowedEntryPath(const QString &entryPath);

    explicit BackupService(DatabaseManager &database, QString dataRoot = QString(),
                           QObject *parent = nullptr);

    QString lastError() const;
    bool busy() const;
    int progress() const;
    QString statusText() const;

    /// Starts an asynchronous backup. Returns false when busy or invalid.
    Q_INVOKABLE bool createBackup(const QString &destinationPath);
    /// Starts an asynchronous restore.
    Q_INVOKABLE bool restoreBackup(const QString &sourcePath);

    Q_INVOKABLE QString suggestedBackupFileName() const;
    /// Accepted file URL for the save dialog (backups directory +
    /// suggested name) - static helpers are not visible to QML.
    Q_INVOKABLE QString suggestedBackupFileUrl() const;
    Q_INVOKABLE QString backupFileExtension() const;
    Q_INVOKABLE QString backupsDirectory() const;

signals:
    void backupFinished(bool ok, const QString &error, const QString &path);
    void restoreFinished(bool ok, const QString &error, const QString &safetyCopyPath);
    void lastErrorChanged();
    void busyChanged();
    void progressChanged();
    void statusTextChanged();

private:
    bool fail(const QString &message);
    void clearError();
    void setBusy(bool busy);
    void setProgress(int value);
    void setStatusText(const QString &text);
    void finishBackup(bool ok, const QString &error, const QString &path);
    void finishRestore(bool ok, const QString &error, const QString &safetyCopyPath);
    void handleStagedRestore(bool ok, const QString &error, const QString &stagingDirectory);
    QString resolveLocalPath(const QString &pathOrUrl) const;
    static void removeDirectoryTree(const QString &path);

    DatabaseManager &m_database;
    QString m_dataRoot;
    QString m_lastError;
    QString m_statusText;
    bool m_busy = false;
    int m_progress = 0;
};

} // namespace talkbox
