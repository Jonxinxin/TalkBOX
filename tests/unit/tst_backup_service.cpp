#include "TestUtils.h"

#include "data/CategoryRepository.h"
#include "data/SnippetRepository.h"
#include "data/Migrations.h"
#include "services/BackupService.h"

#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

using namespace talkbox;

namespace {

struct BackupSetup
{
    testutils::TestEnv env;
    SnippetRepository snippets{env.database};
    CategoryRepository categories{env.database};
    BackupService backup{env.database, env.m_tempDir.path()};

    QString dataRoot() const { return env.m_tempDir.path(); }

    bool placeAttachment(const QString &name, QString *relativeOut = nullptr)
    {
        const QString relative = QStringLiteral("attachments/") + name;
        const QString absolute = dataRoot() + QLatin1Char('/') + relative;
        if (!QDir().mkpath(dataRoot() + QStringLiteral("/attachments")))
            return false;
        QFile file(absolute);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        file.write(QByteArrayLiteral("attachment-payload"));
        file.close();
        if (relativeOut)
            *relativeOut = relative;
        return true;
    }

    QString insertText(const QString &content)
    {
        Snippet snippet;
        snippet.type = QStringLiteral("text");
        snippet.content = content;
        snippet.categoryId = categoryId();
        snippets.insert(snippet);
        return snippet.id;
    }

    /// Categories are user created; the backup fixtures need one so the
    /// category side of the container is exercised too.
    QString categoryId()
    {
        if (!m_categoryId.isEmpty())
            return m_categoryId;
        Category created;
        if (categories.create(QStringLiteral("开场"), QString(), &created))
            m_categoryId = created.id;
        return m_categoryId;
    }

    int snippetCount() const { return snippets.list(SnippetFilter{}).size(); }

private:
    QString m_categoryId;
};

bool runBackup(BackupService &service, const QString &destination, QString *error)
{
    QSignalSpy spy(&service, &BackupService::backupFinished);
    if (!service.createBackup(destination)) {
        *error = service.lastError();
        return false;
    }
    if (spy.isEmpty() && !spy.wait(30000)) {
        *error = QStringLiteral("backup timeout");
        return false;
    }
    const QList<QVariant> arguments = spy.takeFirst();
    *error = arguments.value(1).toString();
    return arguments.value(0).toBool();
}

bool runRestore(BackupService &service, const QString &source, QString *error,
                QString *safetyCopyPath = nullptr)
{
    QSignalSpy spy(&service, &BackupService::restoreFinished);
    if (!service.restoreBackup(source)) {
        *error = service.lastError();
        return false;
    }
    if (spy.isEmpty() && !spy.wait(30000)) {
        *error = QStringLiteral("restore timeout");
        return false;
    }
    const QList<QVariant> arguments = spy.takeFirst();
    *error = arguments.value(1).toString();
    if (safetyCopyPath)
        *safetyCopyPath = arguments.value(2).toString();
    return arguments.value(0).toBool();
}

/// Writes a zip with the given entries (crafted containers for negative
/// tests).
bool writeZip(const QString &path, const QList<QPair<QString, QByteArray>> &entries)
{
    QFile::remove(path);
    QZipWriter writer(path);
    for (const auto &entry : entries)
        writer.addFile(entry.first, entry.second);
    writer.close();
    return writer.status() == QZipWriter::NoError && QFileInfo::exists(path)
        && QFileInfo(path).size() > 0;
}

QByteArray validManifest(int snippetCount, const QStringList &attachmentPaths = {})
{
    QJsonObject manifest;
    manifest.insert(QStringLiteral("formatVersion"), BackupService::kFormatVersion);
    manifest.insert(QStringLiteral("appVersion"), QStringLiteral("test"));
    manifest.insert(QStringLiteral("createdAt"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    QJsonObject counts;
    counts.insert(QStringLiteral("snippets"), snippetCount);
    manifest.insert(QStringLiteral("counts"), counts);
    QJsonArray attachments;
    for (const QString &path : attachmentPaths) {
        QJsonObject object;
        object.insert(QStringLiteral("path"), path);
        object.insert(QStringLiteral("size"), 18);
        attachments.append(object);
    }
    manifest.insert(QStringLiteral("attachments"), attachments);
    return QJsonDocument(manifest).toJson(QJsonDocument::Indented);
}

} // namespace

class TestBackupService : public QObject
{
    Q_OBJECT

private slots:
    void createsBackupWithManifestDatabaseAndAttachments();
    void restoreRoundTripReplacesDataAndKeepsSafetyCopy();
    void rejectsNonZipFile();
    void rejectsMissingManifest();
    void rejectsUnsupportedFormatVersion();
    void rejectsCorruptDatabase();
    void rejectsManifestCountMismatch();
    void rejectsUnsafeEntryPaths();
    void pathGuardsRejectTraversalAndAbsolutePaths();
    void failedRestoreKeepsCurrentDataIntact();
    void backupCanOverwriteExistingFile();
};

void TestBackupService::createsBackupWithManifestDatabaseAndAttachments()
{
    BackupSetup setup;
    QVERIFY(setup.env.init());
    QVERIFY(setup.placeAttachment(QStringLiteral("poster.png")));
    setup.insertText(QStringLiteral("备份内容"));

    const QString backupPath = setup.dataRoot() + QStringLiteral("/test-backup.talkboxbackup");
    QString error;
    QVERIFY2(runBackup(setup.backup, backupPath, &error), qPrintable(error));
    QVERIFY(QFileInfo::exists(backupPath));
    QVERIFY(QFileInfo(backupPath).size() > 0);
    QVERIFY(setup.backup.lastError().isEmpty());

    QZipReader reader(backupPath);
    QVERIFY(reader.isReadable());
    QStringList names;
    for (const QZipReader::FileInfo &info : reader.fileInfoList())
        names << info.filePath;
    QVERIFY(names.contains(BackupService::manifestEntry()));
    QVERIFY(names.contains(BackupService::databaseEntry()));
    QVERIFY(names.contains(QStringLiteral("attachments/poster.png")));

    const QJsonDocument manifest =
        QJsonDocument::fromJson(reader.fileData(BackupService::manifestEntry()));
    QVERIFY(manifest.isObject());
    const QJsonObject object = manifest.object();
    QCOMPARE(object.value(QStringLiteral("formatVersion")).toInt(), BackupService::kFormatVersion);
    QCOMPARE(object.value(QStringLiteral("counts")).toObject().value(QStringLiteral("snippets")).toInt(),
             1);
    const QJsonArray attachments = object.value(QStringLiteral("attachments")).toArray();
    QCOMPARE(attachments.size(), 1);
    QCOMPARE(attachments.first().toObject().value(QStringLiteral("path")).toString(),
             QStringLiteral("attachments/poster.png"));
    QVERIFY(!object.value(QStringLiteral("appVersion")).toString().isEmpty()
            || object.contains(QStringLiteral("appVersion")));
    QVERIFY(!object.value(QStringLiteral("createdAt")).toString().isEmpty());
}

void TestBackupService::restoreRoundTripReplacesDataAndKeepsSafetyCopy()
{
    BackupSetup setup;
    QVERIFY(setup.env.init());
    QString relative;
    QVERIFY(setup.placeAttachment(QStringLiteral("keep.png"), &relative));
    const QString backupSnippetId = setup.insertText(QStringLiteral("备份时的内容"));

    const QString backupPath = setup.dataRoot() + QStringLiteral("/roundtrip.talkboxbackup");
    QString error;
    QVERIFY2(runBackup(setup.backup, backupPath, &error), qPrintable(error));

    // Mutate the live data: remove the snippet, delete the attachment and
    // add a new snippet that must not survive the restore.
    QVERIFY(setup.snippets.remove(backupSnippetId, &error));
    QVERIFY(QFile::remove(setup.dataRoot() + QLatin1Char('/') + relative));
    setup.insertText(QStringLiteral("恢复后应消失"));
    QCOMPARE(setup.snippetCount(), 1);

    QString safetyPath;
    QVERIFY2(runRestore(setup.backup, backupPath, &error, &safetyPath), qPrintable(error));
    QVERIFY(error.isEmpty());

    const QList<Snippet> restored = setup.snippets.list(SnippetFilter{});
    QCOMPARE(restored.size(), 1);
    QCOMPARE(restored.first().content, QStringLiteral("备份时的内容"));
    QCOMPARE(restored.first().id, backupSnippetId);
    QVERIFY(QFileInfo::exists(setup.dataRoot() + QLatin1Char('/') + relative));

    // Previous data was preserved before the swap.
    QVERIFY(!safetyPath.isEmpty());
    QVERIFY(QFileInfo::exists(safetyPath));
    QVERIFY(QFileInfo::exists(safetyPath + QStringLiteral("/talkbox.db")));

    // The database is usable again (reopened) after the restore.
    QCOMPARE(setup.snippetCount(), 1);
}

void TestBackupService::rejectsNonZipFile()
{
    BackupSetup setup;
    QVERIFY(setup.env.init());
    setup.insertText(QStringLiteral("原有内容"));

    const QString bogus = setup.dataRoot() + QStringLiteral("/not-a-backup.talkboxbackup");
    QFile file(bogus);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArrayLiteral("this is not a zip archive"));
    file.close();

    QString error;
    QVERIFY(!runRestore(setup.backup, bogus, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(setup.snippetCount(), 1);
    QCOMPARE(setup.snippets.list(SnippetFilter{}).first().content, QStringLiteral("原有内容"));
}

void TestBackupService::rejectsMissingManifest()
{
    BackupSetup setup;
    QVERIFY(setup.env.init());
    setup.insertText(QStringLiteral("原有内容"));

    // A valid-looking database without the manifest.
    const QString goodBackup = setup.dataRoot() + QStringLiteral("/good.talkboxbackup");
    QString error;
    QVERIFY2(runBackup(setup.backup, goodBackup, &error), qPrintable(error));
    const QByteArray databaseBytes = QZipReader(goodBackup).fileData(BackupService::databaseEntry());
    QVERIFY(!databaseBytes.isEmpty());

    const QString crafted = setup.dataRoot() + QStringLiteral("/no-manifest.talkboxbackup");
    QVERIFY(writeZip(crafted, {{BackupService::databaseEntry(), databaseBytes}}));

    QVERIFY(!runRestore(setup.backup, crafted, &error));
    QVERIFY(error.contains(QStringLiteral("manifest")));
    QCOMPARE(setup.snippetCount(), 1);
}

void TestBackupService::rejectsUnsupportedFormatVersion()
{
    BackupSetup setup;
    QVERIFY(setup.env.init());
    setup.insertText(QStringLiteral("原有内容"));

    const QString goodBackup = setup.dataRoot() + QStringLiteral("/good.talkboxbackup");
    QString error;
    QVERIFY2(runBackup(setup.backup, goodBackup, &error), qPrintable(error));
    QZipReader reader(goodBackup);
    const QByteArray databaseBytes = reader.fileData(BackupService::databaseEntry());

    QJsonObject manifest;
    manifest.insert(QStringLiteral("formatVersion"), 99);
    manifest.insert(QStringLiteral("counts"), QJsonObject{{QStringLiteral("snippets"), 1}});
    const QString crafted = setup.dataRoot() + QStringLiteral("/future-version.talkboxbackup");
    QVERIFY(writeZip(crafted, {{BackupService::manifestEntry(),
                                QJsonDocument(manifest).toJson()},
                               {BackupService::databaseEntry(), databaseBytes}}));

    QVERIFY(!runRestore(setup.backup, crafted, &error));
    QVERIFY(error.contains(QStringLiteral("版本")));
    QCOMPARE(setup.snippetCount(), 1);
    QVERIFY(setup.backup.lastError().contains(QStringLiteral("版本")));
}

void TestBackupService::rejectsCorruptDatabase()
{
    BackupSetup setup;
    QVERIFY(setup.env.init());
    setup.insertText(QStringLiteral("原有内容"));

    const QString crafted = setup.dataRoot() + QStringLiteral("/corrupt.talkboxbackup");
    QVERIFY(writeZip(crafted, {{BackupService::manifestEntry(), validManifest(0)},
                               {BackupService::databaseEntry(),
                                QByteArrayLiteral("definitely not a sqlite database")}}));

    QString error;
    QVERIFY(!runRestore(setup.backup, crafted, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(setup.snippetCount(), 1);
    QCOMPARE(setup.snippets.list(SnippetFilter{}).first().content, QStringLiteral("原有内容"));
}

void TestBackupService::rejectsManifestCountMismatch()
{
    BackupSetup setup;
    QVERIFY(setup.env.init());
    setup.insertText(QStringLiteral("原有内容"));

    const QString goodBackup = setup.dataRoot() + QStringLiteral("/good.talkboxbackup");
    QString error;
    QVERIFY2(runBackup(setup.backup, goodBackup, &error), qPrintable(error));
    const QByteArray databaseBytes = QZipReader(goodBackup).fileData(BackupService::databaseEntry());

    // Manifest claims 99 snippets, the database holds 1.
    const QString crafted = setup.dataRoot() + QStringLiteral("/mismatch.talkboxbackup");
    QVERIFY(writeZip(crafted, {{BackupService::manifestEntry(), validManifest(99)},
                               {BackupService::databaseEntry(), databaseBytes}}));

    QVERIFY(!runRestore(setup.backup, crafted, &error));
    QVERIFY(error.contains(QStringLiteral("不完整")));
    QCOMPARE(setup.snippetCount(), 1);
}

void TestBackupService::rejectsUnsafeEntryPaths()
{
    BackupSetup setup;
    QVERIFY(setup.env.init());
    setup.insertText(QStringLiteral("原有内容"));

    const QString crafted = setup.dataRoot() + QStringLiteral("/traversal.talkboxbackup");
    QVERIFY(writeZip(crafted, {{BackupService::manifestEntry(), validManifest(1)},
                               {BackupService::databaseEntry(), QByteArrayLiteral("x")},
                               {QStringLiteral("../evil.txt"), QByteArrayLiteral("evil")}}));

    QString error;
    QVERIFY(!runRestore(setup.backup, crafted, &error));
    QVERIFY(!error.isEmpty());
    // Nothing was written outside the staging area.
    QVERIFY(!QFileInfo::exists(setup.dataRoot() + QStringLiteral("/evil.txt")));
    QCOMPARE(setup.snippetCount(), 1);
}

void TestBackupService::pathGuardsRejectTraversalAndAbsolutePaths()
{
    QVERIFY(BackupService::isSafeEntryPath(QStringLiteral("manifest.json")));
    QVERIFY(BackupService::isSafeEntryPath(QStringLiteral("attachments/a.png")));
    QVERIFY(BackupService::isSafeEntryPath(QStringLiteral("attachments/sub/b.pdf")));
    QVERIFY(!BackupService::isSafeEntryPath(QString()));
    QVERIFY(!BackupService::isSafeEntryPath(QStringLiteral("../evil.txt")));
    QVERIFY(!BackupService::isSafeEntryPath(QStringLiteral("attachments/../../evil.txt")));
    QVERIFY(!BackupService::isSafeEntryPath(QStringLiteral("/absolute/x")));
    QVERIFY(!BackupService::isSafeEntryPath(QStringLiteral("C:/windows/x")));
    QVERIFY(!BackupService::isSafeEntryPath(QStringLiteral("attachments\\..\\x")));
    QVERIFY(!BackupService::isSafeEntryPath(QStringLiteral("./x")));

    QVERIFY(BackupService::isAllowedEntryPath(QStringLiteral("manifest.json")));
    QVERIFY(BackupService::isAllowedEntryPath(QStringLiteral("talkbox.db")));
    QVERIFY(BackupService::isAllowedEntryPath(QStringLiteral("attachments/a.png")));
    QVERIFY(!BackupService::isAllowedEntryPath(QStringLiteral("evil.txt")));
    QVERIFY(!BackupService::isAllowedEntryPath(QStringLiteral("backups/x")));
    QVERIFY(!BackupService::isAllowedEntryPath(QStringLiteral("attachments/../evil.txt")));
    QVERIFY(!BackupService::isAllowedEntryPath(QStringLiteral("other/talkbox.db")));
}

void TestBackupService::failedRestoreKeepsCurrentDataIntact()
{
    BackupSetup setup;
    QVERIFY(setup.env.init());
    QString relative;
    QVERIFY(setup.placeAttachment(QStringLiteral("current.png"), &relative));
    setup.insertText(QStringLiteral("当前内容"));

    // Restore of a corrupt container must not touch the live data.
    const QString crafted = setup.dataRoot() + QStringLiteral("/broken.talkboxbackup");
    QVERIFY(writeZip(crafted, {{BackupService::manifestEntry(), validManifest(0)},
                               {BackupService::databaseEntry(), QByteArrayLiteral("junk")}}));

    QString error;
    QVERIFY(!runRestore(setup.backup, crafted, &error));
    QVERIFY(!error.isEmpty());

    QCOMPARE(setup.snippetCount(), 1);
    QCOMPARE(setup.snippets.list(SnippetFilter{}).first().content, QStringLiteral("当前内容"));
    QVERIFY(QFileInfo::exists(setup.dataRoot() + QLatin1Char('/') + relative));
    // The database stays open and usable.
    QVERIFY(setup.env.database.isOpen());
    // The user-created category survived the rejected restore.
    QCOMPARE(setup.categories.listAll().size(), 1);
}

void TestBackupService::backupCanOverwriteExistingFile()
{
    BackupSetup setup;
    QVERIFY(setup.env.init());
    setup.insertText(QStringLiteral("内容一"));

    const QString backupPath = setup.dataRoot() + QStringLiteral("/overwrite.talkboxbackup");
    QString error;
    QVERIFY2(runBackup(setup.backup, backupPath, &error), qPrintable(error));
    const qint64 firstSize = QFileInfo(backupPath).size();
    QVERIFY(firstSize > 0);

    setup.insertText(QStringLiteral("内容二"));
    QVERIFY2(runBackup(setup.backup, backupPath, &error), qPrintable(error));
    QVERIFY(QFileInfo::exists(backupPath));

    // The new backup holds both snippets.
    const QJsonDocument manifest = QJsonDocument::fromJson(
        QZipReader(backupPath).fileData(BackupService::manifestEntry()));
    QCOMPARE(manifest.object().value(QStringLiteral("counts")).toObject()
                 .value(QStringLiteral("snippets")).toInt(),
             2);
}

QTEST_GUILESS_MAIN(TestBackupService)

#include "tst_backup_service.moc"
