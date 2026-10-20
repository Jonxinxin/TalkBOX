#include "services/BackupService.h"

#include "app/AppPaths.h"
#include "data/DatabaseManager.h"
#include "data/Migrations.h"

#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QUrl>
#include <QVariant>

#include <QtConcurrent/QtConcurrentRun>

namespace talkbox {

namespace {

QString nowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

/// SQLite string literal escaping for VACUUM INTO paths.
QString quoteSqlPath(const QString &path)
{
    QString escaped = path;
    escaped.replace(QLatin1Char('\''), QLatin1String("''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

} // namespace

BackupService::BackupService(DatabaseManager &database, QString dataRoot, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_dataRoot(dataRoot.isEmpty() ? AppPaths::dataDir() : std::move(dataRoot))
{
}

QString BackupService::backupExtension()
{
    return QStringLiteral("talkboxbackup");
}

QString BackupService::manifestEntry()
{
    return QStringLiteral("manifest.json");
}

QString BackupService::databaseEntry()
{
    return QStringLiteral("talkbox.db");
}

QString BackupService::attachmentsEntryRoot()
{
    return QStringLiteral("attachments");
}

bool BackupService::isSafeEntryPath(const QString &entryPath)
{
    if (entryPath.isEmpty() || entryPath.contains(QChar(0)))
        return false;

    QString path = entryPath;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (path.startsWith(QLatin1Char('/')))
        return false; // absolute
    if (path.contains(QLatin1Char(':')))
        return false; // drive letter or NTFS alternate data stream

    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == QLatin1String(".") || part == QLatin1String(".."))
            return false;
    }
    return !parts.isEmpty();
}

bool BackupService::isAllowedEntryPath(const QString &entryPath)
{
    if (!isSafeEntryPath(entryPath))
        return false;
    const QString normalized = QDir::cleanPath(entryPath);
    if (normalized == manifestEntry() || normalized == databaseEntry())
        return true;
    return normalized.startsWith(attachmentsEntryRoot() + QLatin1Char('/'));
}

QString BackupService::lastError() const
{
    return m_lastError;
}

bool BackupService::busy() const
{
    return m_busy;
}

int BackupService::progress() const
{
    return m_progress;
}

QString BackupService::statusText() const
{
    return m_statusText;
}

QString BackupService::backupsDirectory() const
{
    return m_dataRoot + QStringLiteral("/backups");
}

QString BackupService::suggestedBackupFileName() const
{
    return QStringLiteral("TalkBOX-备份-%1.%2")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")),
             backupExtension());
}

QString BackupService::suggestedBackupFileUrl() const
{
    return QUrl::fromLocalFile(backupsDirectory() + QLatin1Char('/') + suggestedBackupFileName())
        .toString();
}

QString BackupService::backupFileExtension() const
{
    return backupExtension();
}

QString BackupService::resolveLocalPath(const QString &pathOrUrl) const
{
    const QString trimmed = pathOrUrl.trimmed();
    const QUrl url(trimmed);
    if (url.isLocalFile())
        return url.toLocalFile();
    return trimmed;
}

void BackupService::removeDirectoryTree(const QString &path)
{
    if (path.isEmpty())
        return;
    QDir directory(path);
    if (directory.exists())
        directory.removeRecursively();
}

bool BackupService::createBackup(const QString &destinationPath)
{
    if (m_busy) {
        fail(tr("已有备份或恢复任务正在进行。"));
        return false;
    }

    QString destination = resolveLocalPath(destinationPath);
    if (destination.isEmpty()) {
        fail(tr("请选择备份文件的保存位置。"));
        return false;
    }
    if (!destination.endsWith(QLatin1Char('.') + backupExtension(), Qt::CaseInsensitive))
        destination += QLatin1Char('.') + backupExtension();
    destination = QDir::cleanPath(destination);

    if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
        fail(tr("无法创建备份目录：%1").arg(QFileInfo(destination).absolutePath()));
        return false;
    }
    QFile::remove(destination); // overwrite the chosen file

    // Consistent snapshot without touching the live file (PRD F-11).
    const QString snapshotPath =
        m_dataRoot + QStringLiteral("/.backup-snapshot-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".db");
    QFile::remove(snapshotPath);
    {
        QSqlQuery vacuum(m_database.database());
        if (!vacuum.exec(QStringLiteral("VACUUM INTO %1").arg(quoteSqlPath(snapshotPath)))) {
            fail(tr("无法创建数据库快照：%1").arg(vacuum.lastError().text()));
            return false;
        }
    }

    // Collect the attachment list and record counts for the manifest.
    struct AttachmentEntry
    {
        QString absolutePath;
        QString entryName;
        qint64 size = 0;
    };
    QList<AttachmentEntry> attachments;
    const QDir attachmentsDir(m_dataRoot + QStringLiteral("/attachments"));
    const QFileInfoList attachmentFiles =
        attachmentsDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &info : attachmentFiles) {
        attachments.append({info.absoluteFilePath(),
                            attachmentsEntryRoot() + QLatin1Char('/') + info.fileName(),
                            info.size()});
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("formatVersion"), kFormatVersion);
    manifest.insert(QStringLiteral("appVersion"), QCoreApplication::applicationVersion());
    manifest.insert(QStringLiteral("createdAt"), nowIso());
    {
        QJsonObject database;
        database.insert(QStringLiteral("entry"), databaseEntry());
        database.insert(QStringLiteral("size"), QFileInfo(snapshotPath).size());
        manifest.insert(QStringLiteral("database"), database);
    }
    {
        QJsonArray list;
        for (const AttachmentEntry &entry : attachments) {
            QJsonObject object;
            object.insert(QStringLiteral("path"), entry.entryName);
            object.insert(QStringLiteral("size"), entry.size);
            list.append(object);
        }
        manifest.insert(QStringLiteral("attachments"), list);
    }
    {
        QJsonObject counts;
        QSqlQuery query(m_database.database());
        if (query.exec(QStringLiteral("SELECT COUNT(*) FROM snippets")) && query.next())
            counts.insert(QStringLiteral("snippets"), query.value(0).toInt());
        if (query.exec(QStringLiteral("SELECT COUNT(*) FROM categories")) && query.next())
            counts.insert(QStringLiteral("categories"), query.value(0).toInt());
        manifest.insert(QStringLiteral("counts"), counts);
    }
    const QByteArray manifestJson = QJsonDocument(manifest).toJson(QJsonDocument::Indented);

    setBusy(true);
    setProgress(0);
    setStatusText(tr("正在写入备份文件…"));
    clearError();

    qInfo().noquote() << "Backup started:" << destination << "attachments:"
                      << attachments.size();

    QPointer<BackupService> guard(this);
    (void)QtConcurrent::run([guard, destination, snapshotPath, manifestJson, attachments]() {
        QString error;
        bool ok = false;

        {
            const int totalSteps = attachments.size() + 2; // manifest + database + files
            int done = 0;
            QZipWriter writer(destination);
            writer.addFile(BackupService::manifestEntry(), manifestJson);
            ++done;
            {
                QFile databaseFile(snapshotPath);
                if (!databaseFile.open(QIODevice::ReadOnly)) {
                    error = QObject::tr("无法读取数据库快照。");
                } else {
                    writer.addFile(BackupService::databaseEntry(), &databaseFile);
                    databaseFile.close();
                    ++done;
                }
            }

            if (error.isEmpty()) {
                for (const auto &attachment : attachments) {
                    QFile file(attachment.absolutePath);
                    if (!file.open(QIODevice::ReadOnly)) {
                        error = QObject::tr("无法读取附件：%1").arg(attachment.entryName);
                        break;
                    }
                    writer.addFile(attachment.entryName, &file);
                    file.close();
                    ++done;
                    if (guard) {
                        const int percent = totalSteps > 0 ? (done * 100) / totalSteps : 100;
                        QMetaObject::invokeMethod(
                            guard,
                            [guard, percent]() {
                                if (guard)
                                    guard->setProgress(percent);
                            },
                            Qt::QueuedConnection);
                    }
                }
            }

            writer.close();
            if (error.isEmpty() && writer.status() != QZipWriter::NoError)
                error = QObject::tr("写入备份文件失败（磁盘空间或权限不足）。");
            ok = error.isEmpty() && QFileInfo::exists(destination)
                && QFileInfo(destination).size() > 0;
        }

        QFile::remove(snapshotPath);

        if (!ok) {
            QFile::remove(destination); // never leave a partial backup behind
        }

        if (!guard)
            return;
        QMetaObject::invokeMethod(
            guard,
            [guard, ok, error, destination]() {
                if (guard)
                    guard->finishBackup(ok, error, destination);
            },
            Qt::QueuedConnection);
    });

    return true;
}

void BackupService::finishBackup(bool ok, const QString &error, const QString &path)
{
    setBusy(false);
    if (ok) {
        setProgress(100);
        setStatusText(tr("备份完成"));
        clearError();
        qInfo().noquote() << "Backup finished:" << path << QFileInfo(path).size() << "bytes";
    } else {
        setProgress(0);
        setStatusText(tr("备份失败"));
        fail(error.isEmpty() ? tr("备份失败。") : error);
        qWarning().noquote() << "Backup failed:" << error;
    }
    emit backupFinished(ok, error, path);
}

bool BackupService::restoreBackup(const QString &sourcePath)
{
    if (m_busy) {
        fail(tr("已有备份或恢复任务正在进行。"));
        return false;
    }

    const QString source = QDir::cleanPath(resolveLocalPath(sourcePath));
    const QFileInfo sourceInfo(source);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        fail(tr("备份文件不存在：%1").arg(source));
        return false;
    }
    if (sourceInfo.size() <= 0) {
        fail(tr("备份文件为空。"));
        return false;
    }

    setBusy(true);
    setProgress(0);
    setStatusText(tr("正在校验备份文件…"));
    clearError();
    qInfo().noquote() << "Restore started from:" << source;

    const QString stagingDirectory = m_dataRoot + QStringLiteral("/.restore-staging-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);

    QPointer<BackupService> guard(this);
    (void)QtConcurrent::run([guard, source, stagingDirectory]() {
        QString error;
        bool ok = false;

        auto reportProgress = [guard](int percent) {
            if (!guard)
                return;
            QMetaObject::invokeMethod(
                guard,
                [guard, percent]() {
                    if (guard)
                        guard->setProgress(percent);
                },
                Qt::QueuedConnection);
        };
        auto reportStatus = [guard](const QString &text) {
            if (!guard)
                return;
            QMetaObject::invokeMethod(
                guard,
                [guard, text]() {
                    if (guard)
                        guard->setStatusText(text);
                },
                Qt::QueuedConnection);
        };

        QZipReader reader(source);
        if (!reader.isReadable()) {
            error = QObject::tr("不是有效的备份文件（无法读取压缩包）。");
        } else {
            const QList<QZipReader::FileInfo> entries = reader.fileInfoList();
            QByteArray manifestJson;
            bool hasDatabase = false;
            QList<QZipReader::FileInfo> filesToExtract;
            bool pathIssue = false;
            QString badEntry;

            for (const QZipReader::FileInfo &info : entries) {
                if (!BackupService::isAllowedEntryPath(info.filePath)) {
                    pathIssue = true;
                    badEntry = info.filePath;
                    break;
                }
                if (info.isDir)
                    continue;
                filesToExtract.append(info);
                if (info.filePath == BackupService::manifestEntry())
                    manifestJson = reader.fileData(info.filePath);
                else if (info.filePath == BackupService::databaseEntry())
                    hasDatabase = true;
            }

            if (pathIssue) {
                error = QObject::tr("备份文件包含非法路径，已拒绝恢复：%1").arg(badEntry);
            } else if (manifestJson.isEmpty()) {
                error = QObject::tr("备份缺少 manifest.json，可能不是 TalkBOX 备份文件。");
            } else if (!hasDatabase) {
                error = QObject::tr("备份缺少数据库文件。");
            } else {
                QJsonParseError parseError;
                const QJsonDocument manifest = QJsonDocument::fromJson(manifestJson, &parseError);
                if (parseError.error != QJsonParseError::NoError || !manifest.isObject()) {
                    error = QObject::tr("备份清单损坏：%1").arg(parseError.errorString());
                } else {
                    const QJsonObject object = manifest.object();
                    const int formatVersion =
                        object.value(QStringLiteral("formatVersion")).toInt(-1);
                    if (formatVersion != BackupService::kFormatVersion) {
                        error = QObject::tr(
                                    "备份格式版本(%1)不受当前软件支持（支持版本 %2）。")
                                    .arg(formatVersion)
                                    .arg(BackupService::kFormatVersion);
                    } else {
                        reportStatus(QObject::tr("正在解压备份…"));
                        BackupService::removeDirectoryTree(stagingDirectory);
                        const int totalFiles = qMax(1, filesToExtract.size());
                        int done = 0;
                        bool writeFailed = false;

                        for (const QZipReader::FileInfo &info : filesToExtract) {
                            const QString target =
                                stagingDirectory + QLatin1Char('/') + info.filePath;
                            if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
                                error = QObject::tr("无法创建暂存目录。");
                                writeFailed = true;
                                break;
                            }
                            QFile file(target);
                            if (!file.open(QIODevice::WriteOnly)
                                || file.write(reader.fileData(info.filePath)) < 0) {
                                error = QObject::tr("写入暂存文件失败：%1").arg(info.filePath);
                                writeFailed = true;
                                break;
                            }
                            file.close();
                            ++done;
                            reportProgress((done * 70) / totalFiles);
                        }

                        if (!writeFailed) {
                            reportStatus(QObject::tr("正在校验备份数据…"));
                            reportProgress(75);

                            // Verify the staged database on its own connection.
                            const QString connectionName =
                                QStringLiteral("talkbox_restore_check_")
                                + QUuid::createUuid().toString(QUuid::WithoutBraces);
                            bool dbOk = false;
                            QString dbError;
                            {
                                QSqlDatabase check =
                                    QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                              connectionName);
                                check.setDatabaseName(stagingDirectory + QLatin1Char('/')
                                                      + BackupService::databaseEntry());
                                if (!check.open()) {
                                    dbError = QObject::tr("备份中的数据库无法打开：%1")
                                                  .arg(check.lastError().text());
                                } else {
                                    QSqlQuery query(check);
                                    int version = 0;
                                    if (query.exec(QStringLiteral("PRAGMA user_version"))
                                        && query.next()) {
                                        version = query.value(0).toInt();
                                    }
                                    QString integrity;
                                    if (query.exec(QStringLiteral("PRAGMA integrity_check"))
                                        && query.next()) {
                                        integrity = query.value(0).toString();
                                    }
                                    int snippetCount = 0;
                                    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM snippets"))
                                        && query.next()) {
                                        snippetCount = query.value(0).toInt();
                                    }
                                    const int expectedSnippets =
                                        object.value(QStringLiteral("counts"))
                                            .toObject()
                                            .value(QStringLiteral("snippets"))
                                            .toInt(-1);

                                    if (version != migrations::kTargetVersion) {
                                        dbError = QObject::tr(
                                                      "备份数据版本(v%1)与当前软件(v%2)不一致。")
                                                      .arg(version)
                                                      .arg(migrations::kTargetVersion);
                                    } else if (integrity != QLatin1String("ok")) {
                                        dbError = QObject::tr("备份数据库完整性校验失败：%1")
                                                      .arg(integrity);
                                    } else if (expectedSnippets >= 0
                                               && snippetCount != expectedSnippets) {
                                        dbError = QObject::tr(
                                                      "备份内容不完整（清单 %1 条，实际 %2 条）。")
                                                      .arg(expectedSnippets)
                                                      .arg(snippetCount);
                                    } else {
                                        dbOk = true;
                                    }
                                    check.close();
                                }
                            }
                            QSqlDatabase::removeDatabase(connectionName);

                            if (!dbOk) {
                                error = dbError;
                            } else {
                                // Every manifest attachment must be present.
                                const QJsonArray listedAttachments =
                                    object.value(QStringLiteral("attachments")).toArray();
                                for (const QJsonValue &value : listedAttachments) {
                                    const QString path = value.toObject()
                                                             .value(QStringLiteral("path"))
                                                             .toString();
                                    if (path.isEmpty()
                                        || !QFileInfo::exists(stagingDirectory + QLatin1Char('/')
                                                              + path)) {
                                        error = QObject::tr("备份缺少附件文件：%1").arg(path);
                                        break;
                                    }
                                }
                                if (error.isEmpty())
                                    ok = true;
                            }
                        }
                    }
                }
            }
        }

        if (!ok)
            BackupService::removeDirectoryTree(stagingDirectory);

        if (!guard)
            return;
        QMetaObject::invokeMethod(
            guard,
            [guard, ok, error, stagingDirectory]() {
                if (guard)
                    guard->handleStagedRestore(ok, error, stagingDirectory);
            },
            Qt::QueuedConnection);
    });

    return true;
}

void BackupService::handleStagedRestore(bool ok, const QString &error,
                                        const QString &stagingDirectory)
{
    if (!ok) {
        setBusy(false);
        setProgress(0);
        setStatusText(tr("恢复失败"));
        fail(error.isEmpty() ? tr("恢复失败。") : error);
        qWarning().noquote() << "Restore rejected:" << error;
        removeDirectoryTree(stagingDirectory);
        finishRestore(false, error, QString());
        return;
    }

    setStatusText(tr("正在替换数据…"));
    setProgress(85);

    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString safetyDirectory = backupsDirectory() + QStringLiteral("/pre-restore-") + stamp;
    if (!QDir().mkpath(safetyDirectory)) {
        removeDirectoryTree(stagingDirectory);
        setBusy(false);
        setProgress(0);
        setStatusText(tr("恢复失败"));
        fail(tr("无法创建安全备份目录：%1").arg(safetyDirectory));
        finishRestore(false, m_lastError, QString());
        return;
    }

    const QString liveDatabase = m_database.filePath().isEmpty()
        ? m_dataRoot + QStringLiteral("/talkbox.db")
        : m_database.filePath();
    const QString liveAttachments = m_dataRoot + QStringLiteral("/attachments");
    const QString safetyDatabase = safetyDirectory + QStringLiteral("/talkbox.db");
    const QString safetyAttachments = safetyDirectory + QStringLiteral("/attachments");
    const QString stagedDatabase = stagingDirectory + QStringLiteral("/talkbox.db");
    const QString stagedAttachments = stagingDirectory + QStringLiteral("/attachments");

    m_database.close();

    auto moveFile = [](const QString &from, const QString &to) {
        if (!QFileInfo::exists(from))
            return true;
        QFile::remove(to);
        return QFile::rename(from, to);
    };

    bool movedOut = moveFile(liveDatabase, safetyDatabase);
    for (const QString &suffix : {QStringLiteral("-wal"), QStringLiteral("-shm")}) {
        const QString leftover = liveDatabase + suffix;
        if (QFileInfo::exists(leftover))
            QFile::remove(leftover);
    }
    if (movedOut && QDir(liveAttachments).exists())
        movedOut = QDir().rename(liveAttachments, safetyAttachments);

    bool movedIn = movedOut && moveFile(stagedDatabase, liveDatabase);
    if (movedIn && QDir(stagedAttachments).exists())
        movedIn = QDir().rename(stagedAttachments, liveAttachments);
    if (movedIn && !QDir(liveAttachments).exists())
        movedIn = QDir().mkpath(liveAttachments);

    auto rollback = [&]() {
        removeDirectoryTree(liveAttachments);
        QFile::remove(liveDatabase);
        moveFile(safetyDatabase, liveDatabase);
        if (QDir(safetyAttachments).exists())
            QDir().rename(safetyAttachments, liveAttachments);
        QString ignored;
        m_database.open(liveDatabase, &ignored);
    };

    if (!movedIn) {
        rollback();
        removeDirectoryTree(stagingDirectory);
        setBusy(false);
        setProgress(0);
        setStatusText(tr("恢复失败"));
        fail(tr("替换数据文件失败，已还原原数据。"));
        qWarning().noquote() << "Restore swap failed; original data restored.";
        finishRestore(false, m_lastError, QString());
        return;
    }

    QString openError;
    if (!m_database.open(liveDatabase, &openError)) {
        rollback();
        removeDirectoryTree(stagingDirectory);
        setBusy(false);
        setProgress(0);
        setStatusText(tr("恢复失败"));
        fail(tr("恢复后的数据库无法打开，已还原原数据：%1").arg(openError));
        qWarning().noquote() << "Restore reopen failed:" << openError;
        finishRestore(false, m_lastError, QString());
        return;
    }

    removeDirectoryTree(stagingDirectory);
    setBusy(false);
    setProgress(100);
    setStatusText(tr("恢复完成"));
    clearError();
    qInfo().noquote() << "Restore finished. Previous data preserved at:" << safetyDirectory;
    finishRestore(true, QString(), safetyDirectory);
}

void BackupService::finishRestore(bool ok, const QString &error, const QString &safetyCopyPath)
{
    setBusy(false);
    emit restoreFinished(ok, error, safetyCopyPath);
}

bool BackupService::fail(const QString &message)
{
    if (m_lastError != message) {
        m_lastError = message;
        emit lastErrorChanged();
    }
    return false;
}

void BackupService::clearError()
{
    if (m_lastError.isEmpty())
        return;
    m_lastError.clear();
    emit lastErrorChanged();
}

void BackupService::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void BackupService::setProgress(int value)
{
    value = qBound(0, value, 100);
    if (m_progress == value)
        return;
    m_progress = value;
    emit progressChanged();
}

void BackupService::setStatusText(const QString &text)
{
    if (m_statusText == text)
        return;
    m_statusText = text;
    emit statusTextChanged();
}

} // namespace talkbox
