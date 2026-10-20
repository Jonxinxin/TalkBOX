#include "TestUtils.h"

#include "services/AttachmentService.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QTest>

using talkbox::AttachmentService;

namespace {

QString writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return QString();
    file.write(content);
    file.close();
    return path;
}

QString writePng(const QString &path, int size = 8)
{
    QImage image(size, size, QImage::Format_RGB32);
    image.fill(Qt::red);
    if (!image.save(path, "PNG"))
        return QString();
    return path;
}

/// Runs an import to completion and returns the result of the first
/// matching importFinished() signal: [ok, error, relativePath, mime, size].
QList<QVariant> runImport(AttachmentService &service, const QString &path, const QString &type)
{
    QSignalSpy spy(&service, &AttachmentService::importFinished);
    const QString token = service.startImport(path, type);
    if (token.isEmpty())
        return {false, service.lastError(), QString(), QString(), -1LL};
    if (spy.isEmpty() && !spy.wait(10000))
        return {false, QStringLiteral("timeout"), QString(), QString(), -1LL};
    const QList<QVariant> arguments = spy.takeFirst();
    if (arguments.value(0).toString() != token)
        return {false, QStringLiteral("token mismatch"), QString(), QString(), -1LL};
    return {arguments.value(1), arguments.value(2), arguments.value(3), arguments.value(4),
            arguments.value(5)};
}

} // namespace

class TestAttachmentService : public QObject
{
    Q_OBJECT

private slots:
    void importsImageAndStoresUniqueFileName();
    void duplicateImportsGetUniqueStorageNames();
    void rejectsUnknownTypeAndMissingFile();
    void rejectsWrongExtension();
    void rejectsOversizeImage();
    void rejectsEmptyFile();
    void rejectsCorruptImage();
    void rejectsPdfWithoutMagicHeader();
    void acceptsValidPdfHeader();
    void removeManagedFileWorksAndIsSafe();
    void absolutePathResolvesInsideAttachmentsOnly();
    void fileSizeForWorks();
};

void TestAttachmentService::importsImageAndStoresUniqueFileName()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    const QString source = writePng(env.m_tempDir.filePath(QStringLiteral("source.png")));
    QVERIFY(!source.isEmpty());

    AttachmentService service(env.m_tempDir.path());
    const QList<QVariant> result = runImport(service, source, QStringLiteral("image"));
    QVERIFY2(result.value(0).toBool(), qPrintable(result.value(1).toString()));

    const QString relativePath = result.value(2).toString();
    QVERIFY(relativePath.startsWith(QStringLiteral("attachments/")));
    QVERIFY(relativePath.endsWith(QStringLiteral(".png")));
    QCOMPARE(result.value(3).toString(), QStringLiteral("image/png"));
    QVERIFY(result.value(4).toLongLong() > 0);

    // The stored name must be a UUID, not the original file name.
    QVERIFY(!relativePath.contains(QStringLiteral("source")));
    const QString absolute = service.absolutePath(relativePath);
    QVERIFY(QFileInfo::exists(absolute));

    // Bytes must be identical to the source.
    QFile sourceFile(source);
    QFile storedFile(absolute);
    QVERIFY(sourceFile.open(QIODevice::ReadOnly));
    QVERIFY(storedFile.open(QIODevice::ReadOnly));
    QCOMPARE(storedFile.readAll(), sourceFile.readAll());
}

void TestAttachmentService::duplicateImportsGetUniqueStorageNames()
{
    testutils::TestEnv env;
    QVERIFY(env.init());

    const QString source = writePng(env.m_tempDir.filePath(QStringLiteral("dup.png")));
    AttachmentService service(env.m_tempDir.path());

    const QString first = runImport(service, source, QStringLiteral("image")).value(2).toString();
    const QString second = runImport(service, source, QStringLiteral("image")).value(2).toString();
    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());
    QVERIFY(first != second);
}

void TestAttachmentService::rejectsUnknownTypeAndMissingFile()
{
    testutils::TestEnv env;
    QVERIFY(env.init());
    AttachmentService service(env.m_tempDir.path());

    QVERIFY(service.startImport(QStringLiteral("whatever.png"), QStringLiteral("video")).isEmpty());
    QVERIFY(!service.lastError().isEmpty());

    QVERIFY(service.startImport(env.m_tempDir.filePath(QStringLiteral("nope.png")),
                                QStringLiteral("image"))
                .isEmpty());
    QVERIFY(!service.lastError().isEmpty());
}

void TestAttachmentService::rejectsWrongExtension()
{
    testutils::TestEnv env;
    QVERIFY(env.init());

    const QString source = writeFile(env.m_tempDir.filePath(QStringLiteral("doc.txt")),
                                     QByteArrayLiteral("hello"));
    AttachmentService service(env.m_tempDir.path());

    QVERIFY(service.startImport(source, QStringLiteral("image")).isEmpty());
    QVERIFY(service.lastError().contains(QStringLiteral("不支持")));
}

void TestAttachmentService::rejectsOversizeImage()
{
    testutils::TestEnv env;
    QVERIFY(env.init());

    const QString path = env.m_tempDir.filePath(QStringLiteral("huge.png"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(talkbox::attachmentrules::kMaxImageBytes + 1024));
    }
    AttachmentService service(env.m_tempDir.path());

    QVERIFY(service.startImport(path, QStringLiteral("image")).isEmpty());
    QVERIFY(service.lastError().contains(QStringLiteral("大小限制")));
}

void TestAttachmentService::rejectsEmptyFile()
{
    testutils::TestEnv env;
    QVERIFY(env.init());

    const QString path = writeFile(env.m_tempDir.filePath(QStringLiteral("empty.png")), {});
    AttachmentService service(env.m_tempDir.path());

    QVERIFY(service.startImport(path, QStringLiteral("image")).isEmpty());
    QVERIFY(!service.lastError().isEmpty());
}

void TestAttachmentService::rejectsCorruptImage()
{
    testutils::TestEnv env;
    QVERIFY(env.init());

    const QString path = writeFile(env.m_tempDir.filePath(QStringLiteral("broken.png")),
                                   QByteArrayLiteral("this is not a png"));
    AttachmentService service(env.m_tempDir.path());

    QVERIFY(service.startImport(path, QStringLiteral("image")).isEmpty());
    QVERIFY(!service.lastError().isEmpty());
}

void TestAttachmentService::rejectsPdfWithoutMagicHeader()
{
    testutils::TestEnv env;
    QVERIFY(env.init());

    const QString path = writeFile(env.m_tempDir.filePath(QStringLiteral("fake.pdf")),
                                   QByteArrayLiteral("not really a pdf"));
    AttachmentService service(env.m_tempDir.path());

    QVERIFY(service.startImport(path, QStringLiteral("pdf")).isEmpty());
    QVERIFY(service.lastError().contains(QStringLiteral("PDF")));
}

void TestAttachmentService::acceptsValidPdfHeader()
{
    testutils::TestEnv env;
    QVERIFY(env.init());

    const QString path = writeFile(env.m_tempDir.filePath(QStringLiteral("ok.pdf")),
                                   QByteArrayLiteral("%PDF-1.4\n%%EOF\n"));
    AttachmentService service(env.m_tempDir.path());

    const QList<QVariant> result = runImport(service, path, QStringLiteral("pdf"));
    QVERIFY2(result.value(0).toBool(), qPrintable(result.value(1).toString()));
    QVERIFY(result.value(2).toString().endsWith(QStringLiteral(".pdf")));
    QCOMPARE(result.value(3).toString(), QStringLiteral("application/pdf"));
}

void TestAttachmentService::removeManagedFileWorksAndIsSafe()
{
    testutils::TestEnv env;
    QVERIFY(env.init());

    const QString source = writePng(env.m_tempDir.filePath(QStringLiteral("delete-me.png")));
    AttachmentService service(env.m_tempDir.path());
    const QString relative = runImport(service, source, QStringLiteral("image")).value(2).toString();
    QVERIFY(!relative.isEmpty());
    const QString absolute = service.absolutePath(relative);
    QVERIFY(QFileInfo::exists(absolute));

    QVERIFY(service.removeManagedFile(relative));
    QVERIFY(!QFileInfo::exists(absolute));
    QVERIFY(service.removeManagedFile(relative)); // idempotent

    // Traversal and non-attachment paths are refused.
    QVERIFY(!service.removeManagedFile(QStringLiteral("attachments/../talkbox.db")));
    QVERIFY(!service.removeManagedFile(QStringLiteral("talkbox.db")));
    QVERIFY(!service.removeManagedFile(QStringLiteral("attachments/..\\evil.txt")));
}

void TestAttachmentService::absolutePathResolvesInsideAttachmentsOnly()
{
    testutils::TestEnv env;
    QVERIFY(env.init());
    AttachmentService service(env.m_tempDir.path());

    const QString absolute = service.absolutePath(QStringLiteral("attachments/x.png"));
    QVERIFY(absolute.startsWith(env.m_tempDir.path()));
    QVERIFY(absolute.endsWith(QStringLiteral("attachments/x.png")));

    QVERIFY(service.absolutePath(QStringLiteral("../escape.png")).isEmpty());
    QVERIFY(service.absolutePath(QStringLiteral("attachments/../x.png")).isEmpty());
}

void TestAttachmentService::fileSizeForWorks()
{
    testutils::TestEnv env;
    QVERIFY(env.init());

    const QByteArray payload = QByteArrayLiteral("%PDF-1.4\n%%EOF\n");
    const QString path = writeFile(env.m_tempDir.filePath(QStringLiteral("size.pdf")), payload);
    AttachmentService service(env.m_tempDir.path());

    QCOMPARE(service.fileSizeFor(path), qint64(payload.size()));
    QCOMPARE(service.fileSizeFor(env.m_tempDir.filePath(QStringLiteral("missing.pdf"))), qint64(-1));
}

QTEST_GUILESS_MAIN(TestAttachmentService)

#include "tst_attachment_service.moc"
