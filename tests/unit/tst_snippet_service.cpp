#include "TestUtils.h"

#include "data/CategoryRepository.h"
#include "data/SnippetRepository.h"
#include "services/AttachmentService.h"
#include "services/ClipboardWriter.h"
#include "services/SnippetService.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTest>

namespace {

class FakeClipboardWriter : public talkbox::ClipboardWriter
{
public:
    bool setText(const QString &text) override
    {
        if (failWrites)
            return false;
        m_text = text;
        return true;
    }

    QString text() const override { return m_text; }

    bool failWrites = false;

private:
    QString m_text;
};

/// Bundles the objects most tests need. Declaration order matters:
/// `env` must outlive everything that references its temporary directory.
struct Setup
{
    testutils::TestEnv env;
    talkbox::CategoryRepository categories{env.database};
    talkbox::SnippetRepository repository{env.database};
    talkbox::AttachmentService attachments{env.m_tempDir.path()};
    FakeClipboardWriter clipboard;
    talkbox::SnippetService service{repository, clipboard, attachments};

    /// Categories are user created now; tests make the ones they need and
    /// reuse the id for the rest of the test.
    QString categoryId(const QString &name)
    {
        for (const talkbox::Category &existing : categories.listAll()) {
            if (existing.name == name)
                return existing.id;
        }
        talkbox::Category created;
        if (!categories.create(name, QString(), &created))
            return QStringLiteral("missing-category");
        return created.id;
    }

    /// Creates a managed attachment file on disk (not a database row).
    QString placeAttachmentFile(const QString &name,
                                const QByteArray &content = QByteArrayLiteral("fake-image"))
    {
        const QString relative = QStringLiteral("attachments/") + name;
        const QString absolute = env.m_tempDir.path() + QLatin1Char('/') + relative;
        if (!QDir().mkpath(env.m_tempDir.path() + QStringLiteral("/attachments")))
            return {};
        QFile file(absolute);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(content);
        file.close();
        return relative;
    }
};

talkbox::Snippet makeText(const QString &content, const QString &categoryId)
{
    talkbox::Snippet snippet;
    snippet.type = QStringLiteral("text");
    snippet.content = content;
    snippet.categoryId = categoryId;
    return snippet;
}

} // namespace

class TestSnippetService : public QObject
{
    Q_OBJECT

private slots:
    // --- text creation (phase A) ---
    void createRejectsBlankContent();
    void createTrimsOuterWhitespaceAndEmits();
    void createFallsBackToUncategorized();
    void createFailsForUnknownCategory();
    void copyWritesFullContentAndEmits();
    void copyUnknownIdFails();
    void copyNonTextFails();
    void copyClipboardFailureSurfacesError();
    void copyRecordsLastUsed();

    // --- attachments (phase B) ---
    void createAttachmentSnippetInsertsRow();
    void createAttachmentSnippetAppendsMissingExtension();
    void createAttachmentSnippetRejectsMissingFile();
    void createAttachmentSnippetRejectsUnknownType();

    // --- editing (phase B) ---
    void updateTextSnippetUpdatesContentAndCategory();
    void updateTextSnippetRejectsEmptyAndNonText();
    void renameAttachmentKeepsManagedPath();
    void renameAttachmentValidatesName();
    void changeCategoryUpdatesAndValidates();

    // --- deletion (phase B) ---
    void deleteSnippetRemovesRowAndFile();
    void deleteSnippetKeepsSharedFile();
    void deleteSnippetWarnsWhenCleanupFails();
};

void TestSnippetService::createRejectsBlankContent()
{
    Setup setup;
    QVERIFY(setup.env.init());

    QVERIFY(!setup.service.createTextSnippet(QStringLiteral("   \n\t "),
                                             QString()));
    QVERIFY(!setup.service.lastError().isEmpty());
    QCOMPARE(setup.repository.list({}).size(), 0);
}

void TestSnippetService::createTrimsOuterWhitespaceAndEmits()
{
    Setup setup;
    QVERIFY(setup.env.init());

    QSignalSpy created(&setup.service, &talkbox::SnippetService::snippetCreated);
    QSignalSpy changed(&setup.service, &talkbox::SnippetService::snippetsChanged);

    QVERIFY(setup.service.createTextSnippet(QStringLiteral("  你好\n世界  "),
                                            setup.categoryId(QStringLiteral("开场"))));
    QCOMPARE(created.count(), 1);
    QCOMPARE(changed.count(), 1);
    QVERIFY(setup.service.lastError().isEmpty());

    const QList<talkbox::Snippet> all = setup.repository.list({});
    QCOMPARE(all.size(), 1);
    QCOMPARE(all.first().content, QStringLiteral("你好\n世界"));
    QCOMPARE(all.first().categoryId, setup.categoryId(QStringLiteral("开场")));
}

void TestSnippetService::createFallsBackToUncategorized()
{
    Setup setup;
    QVERIFY(setup.env.init());

    // Blank / missing category means "未分类", i.e. categoryId is empty.
    QVERIFY(setup.service.createTextSnippet(QStringLiteral("内容"), QStringLiteral("  ")));
    const QList<talkbox::Snippet> all = setup.repository.list({});
    QCOMPARE(all.size(), 1);
    QCOMPARE(all.first().categoryId, QString());
    QVERIFY(all.first().categoryName.isEmpty());
}

void TestSnippetService::createFailsForUnknownCategory()
{
    Setup setup;
    QVERIFY(setup.env.init());

    QVERIFY(!setup.service.createTextSnippet(QStringLiteral("内容"),
                                             QStringLiteral("missing-category")));
    QVERIFY(!setup.service.lastError().isEmpty());
    QCOMPARE(setup.repository.list({}).size(), 0);
}

void TestSnippetService::copyWritesFullContentAndEmits()
{
    Setup setup;
    QVERIFY(setup.env.init());

    const QString longContent = QStringLiteral("第一行\n第二行\n第三行\n第四行\n第五行");
    QVERIFY(setup.service.createTextSnippet(longContent, setup.categoryId(QStringLiteral("报价"))));
    const QString id = setup.repository.list({}).first().id;

    QSignalSpy copied(&setup.service, &talkbox::SnippetService::snippetCopied);
    QVERIFY(setup.service.copySnippet(id));
    QCOMPARE(copied.count(), 1);
    QCOMPARE(setup.clipboard.text(), longContent);
}

void TestSnippetService::copyUnknownIdFails()
{
    Setup setup;
    QVERIFY(setup.env.init());

    QVERIFY(!setup.service.copySnippet(QStringLiteral("no-such-id")));
    QVERIFY(!setup.service.lastError().isEmpty());
    QCOMPARE(setup.clipboard.text(), QString());
}

void TestSnippetService::copyNonTextFails()
{
    Setup setup;
    QVERIFY(setup.env.init());

    const QString relative = setup.placeAttachmentFile(QStringLiteral("a.png"));
    QVERIFY(!relative.isEmpty());
    QVERIFY(setup.service.createAttachmentSnippet(QStringLiteral("image"),
                                                  QStringLiteral("海报.png"), relative,
                                                  QStringLiteral("image/png"), 10,
                                                  setup.categoryId(QStringLiteral("报价"))));
    const QString id = setup.repository.list({}).first().id;

    QVERIFY(!setup.service.copySnippet(id));
    QVERIFY(!setup.service.lastError().isEmpty());
}

void TestSnippetService::copyClipboardFailureSurfacesError()
{
    Setup setup;
    QVERIFY(setup.env.init());
    QVERIFY(setup.service.createTextSnippet(QStringLiteral("内容"), QString()));
    const QString id = setup.repository.list({}).first().id;

    setup.clipboard.failWrites = true;
    QSignalSpy copied(&setup.service, &talkbox::SnippetService::snippetCopied);

    QVERIFY(!setup.service.copySnippet(id));
    QVERIFY(!setup.service.lastError().isEmpty());
    QCOMPARE(copied.count(), 0);
}

void TestSnippetService::copyRecordsLastUsed()
{
    Setup setup;
    QVERIFY(setup.env.init());
    QVERIFY(setup.service.createTextSnippet(QStringLiteral("内容"), QString()));
    const QString id = setup.repository.list({}).first().id;

    QVERIFY(setup.service.copySnippet(id));

    const auto after = setup.repository.findById(id);
    QVERIFY(after.has_value());
    QVERIFY(!after->lastUsedAt.isEmpty());
}

void TestSnippetService::createAttachmentSnippetInsertsRow()
{
    Setup setup;
    QVERIFY(setup.env.init());

    const QString relative = setup.placeAttachmentFile(QStringLiteral("poster.png"));
    QVERIFY(!relative.isEmpty());

    QSignalSpy created(&setup.service, &talkbox::SnippetService::snippetCreated);
    QVERIFY2(setup.service.createAttachmentSnippet(QStringLiteral("image"),
                                                   QStringLiteral("门店海报.png"), relative,
                                                   QStringLiteral("image/png"), 1234,
                                                   setup.categoryId(QStringLiteral("报价"))),
             qPrintable(setup.service.lastError()));
    QCOMPARE(created.count(), 1);

    const auto all = setup.repository.list({});
    QCOMPARE(all.size(), 1);
    const talkbox::Snippet &snippet = all.first();
    QCOMPARE(snippet.type, QStringLiteral("image"));
    QCOMPARE(snippet.displayName, QStringLiteral("门店海报.png"));
    QCOMPARE(snippet.relativePath, relative);
    QCOMPARE(snippet.mimeType, QStringLiteral("image/png"));
    QCOMPARE(snippet.fileSize, qint64(1234));
    QCOMPARE(snippet.categoryName, QStringLiteral("报价"));
}

void TestSnippetService::createAttachmentSnippetAppendsMissingExtension()
{
    Setup setup;
    QVERIFY(setup.env.init());

    const QString relative = setup.placeAttachmentFile(QStringLiteral("photo.jpg"));
    QVERIFY(!relative.isEmpty());
    QVERIFY2(setup.service.createAttachmentSnippet(QStringLiteral("image"),
                                                   QStringLiteral("产品照片"), relative,
                                                   QStringLiteral("image/jpeg"), 10,
                                                   QString()),
             qPrintable(setup.service.lastError()));

    QCOMPARE(setup.repository.list({}).first().displayName, QStringLiteral("产品照片.jpg"));
}

void TestSnippetService::createAttachmentSnippetRejectsMissingFile()
{
    Setup setup;
    QVERIFY(setup.env.init());

    QVERIFY(!setup.service.createAttachmentSnippet(
        QStringLiteral("image"), QStringLiteral("x.png"), QStringLiteral("attachments/gone.png"),
        QStringLiteral("image/png"), 10, QString()));
    QVERIFY(!setup.service.lastError().isEmpty());
    QCOMPARE(setup.repository.list({}).size(), 0);
}

void TestSnippetService::createAttachmentSnippetRejectsUnknownType()
{
    Setup setup;
    QVERIFY(setup.env.init());

    const QString relative = setup.placeAttachmentFile(QStringLiteral("x.png"));
    QVERIFY(!setup.service.createAttachmentSnippet(QStringLiteral("video"),
                                                   QStringLiteral("x.png"), relative,
                                                   QStringLiteral("video/mp4"), 10, QString()));
    QVERIFY(!setup.service.lastError().isEmpty());
}

void TestSnippetService::updateTextSnippetUpdatesContentAndCategory()
{
    Setup setup;
    QVERIFY(setup.env.init());
    QVERIFY(setup.service.createTextSnippet(QStringLiteral("初始内容"),
                                            QString()));
    const QString id = setup.repository.list({}).first().id;
    QTest::qSleep(5);

    QVERIFY2(setup.service.updateTextSnippet(id, QStringLiteral("  更新后的内容  "),
                                             setup.categoryId(QStringLiteral("电表"))),
             qPrintable(setup.service.lastError()));

    const auto after = setup.repository.findById(id);
    QVERIFY(after.has_value());
    QCOMPARE(after->content, QStringLiteral("更新后的内容")); // trimmed
    QCOMPARE(after->categoryName, QStringLiteral("电表"));
}

void TestSnippetService::updateTextSnippetRejectsEmptyAndNonText()
{
    Setup setup;
    QVERIFY(setup.env.init());
    QVERIFY(setup.service.createTextSnippet(QStringLiteral("内容"), QString()));
    const QString textId = setup.repository.list({}).first().id;

    QVERIFY(!setup.service.updateTextSnippet(textId, QStringLiteral("   "), QString()));
    QVERIFY(!setup.service.lastError().isEmpty());
    QCOMPARE(setup.repository.findById(textId)->content, QStringLiteral("内容"));

    const QString relative = setup.placeAttachmentFile(QStringLiteral("a.png"));
    QVERIFY(setup.service.createAttachmentSnippet(QStringLiteral("image"),
                                                  QStringLiteral("a.png"), relative,
                                                  QStringLiteral("image/png"), 10, QString()));
    QString attachmentId;
    for (const talkbox::Snippet &snippet : setup.repository.list({})) {
        if (!snippet.isText())
            attachmentId = snippet.id;
    }
    QVERIFY(!attachmentId.isEmpty());
    QVERIFY(!setup.service.updateTextSnippet(attachmentId, QStringLiteral("新内容"), QString()));
    QVERIFY(!setup.service.lastError().isEmpty());
}

void TestSnippetService::renameAttachmentKeepsManagedPath()
{
    Setup setup;
    QVERIFY(setup.env.init());

    const QString relative = setup.placeAttachmentFile(QStringLiteral("old.png"));
    QVERIFY(setup.service.createAttachmentSnippet(QStringLiteral("image"),
                                                  QStringLiteral("旧名字.png"), relative,
                                                  QStringLiteral("image/png"), 10, QString()));
    const QString id = setup.repository.list({}).first().id;

    QVERIFY2(setup.service.renameAttachment(id, QStringLiteral("  新的名字  ")),
             qPrintable(setup.service.lastError()));

    const auto after = setup.repository.findById(id);
    QVERIFY(after.has_value());
    QCOMPARE(after->displayName, QStringLiteral("新的名字.png")); // extension re-appended
    QCOMPARE(after->relativePath, relative);                      // real path untouched
}

void TestSnippetService::renameAttachmentValidatesName()
{
    Setup setup;
    QVERIFY(setup.env.init());

    const QString relative = setup.placeAttachmentFile(QStringLiteral("r.pdf"));
    QVERIFY(setup.service.createAttachmentSnippet(QStringLiteral("pdf"),
                                                  QStringLiteral("文档.pdf"), relative,
                                                  QStringLiteral("application/pdf"), 10, QString()));
    const QString id = setup.repository.list({}).first().id;

    QVERIFY(!setup.service.renameAttachment(id, QStringLiteral("   ")));
    QVERIFY(!setup.service.lastError().isEmpty());

    QVERIFY(!setup.service.renameAttachment(id, QStringLiteral("bad/name.pdf")));
    QVERIFY(!setup.service.lastError().isEmpty());
    QVERIFY(!setup.service.renameAttachment(id, QStringLiteral("bad\\name.pdf")));
    QVERIFY(!setup.service.renameAttachment(id, QStringLiteral("bad:name.pdf")));

    // The text-snippet case: text cannot be renamed.
    QVERIFY(setup.service.createTextSnippet(QStringLiteral("文本"), QString()));
    for (const talkbox::Snippet &snippet : setup.repository.list({})) {
        if (snippet.isText()) {
            QVERIFY(!setup.service.renameAttachment(snippet.id, QStringLiteral("x")));
            QVERIFY(!setup.service.lastError().isEmpty());
        }
    }
}

void TestSnippetService::changeCategoryUpdatesAndValidates()
{
    Setup setup;
    QVERIFY(setup.env.init());
    const QString aftersale = setup.categoryId(QStringLiteral("售后"));
    QVERIFY(!aftersale.isEmpty());
    QVERIFY(setup.service.createTextSnippet(QStringLiteral("内容"), aftersale));
    const QString id = setup.repository.list({}).first().id;
    QCOMPARE(setup.repository.findById(id)->categoryId, aftersale);

    // An empty target is valid: it removes the category ("未分类").
    QVERIFY2(setup.service.changeCategory(id, QString()),
             qPrintable(setup.service.lastError()));
    QVERIFY(setup.repository.findById(id)->categoryId.isEmpty());

    QVERIFY2(setup.service.changeCategory(id, aftersale),
             qPrintable(setup.service.lastError()));
    QCOMPARE(setup.repository.findById(id)->categoryName, QStringLiteral("售后"));

    QVERIFY(!setup.service.changeCategory(id, QStringLiteral("missing-category")));
    QVERIFY(!setup.service.lastError().isEmpty());
    QCOMPARE(setup.repository.findById(id)->categoryName, QStringLiteral("售后"));
}

void TestSnippetService::deleteSnippetRemovesRowAndFile()
{
    Setup setup;
    QVERIFY(setup.env.init());

    const QString relative = setup.placeAttachmentFile(QStringLiteral("gone.png"));
    const QString absolute = setup.attachments.absolutePath(relative);
    QVERIFY(setup.service.createAttachmentSnippet(QStringLiteral("image"),
                                                  QStringLiteral("gone.png"), relative,
                                                  QStringLiteral("image/png"), 10, QString()));
    const QString id = setup.repository.list({}).first().id;

    QSignalSpy deleted(&setup.service, &talkbox::SnippetService::snippetDeleted);
    QVERIFY2(setup.service.deleteSnippet(id), qPrintable(setup.service.lastError()));
    QCOMPARE(deleted.count(), 1);
    QCOMPARE(setup.repository.list({}).size(), 0);
    QVERIFY(!QFile::exists(absolute));
}

void TestSnippetService::deleteSnippetKeepsSharedFile()
{
    Setup setup;
    QVERIFY(setup.env.init());

    const QString relative = setup.placeAttachmentFile(QStringLiteral("shared.png"));
    const QString absolute = setup.attachments.absolutePath(relative);
    QVERIFY(setup.service.createAttachmentSnippet(QStringLiteral("image"),
                                                  QStringLiteral("shared.png"), relative,
                                                  QStringLiteral("image/png"), 10, QString()));

    // A second record referencing the same managed file (defensive case).
    talkbox::Snippet duplicate;
    duplicate.type = QStringLiteral("image");
    duplicate.displayName = QStringLiteral("shared copy.png");
    duplicate.relativePath = relative;
    duplicate.mimeType = QStringLiteral("image/png");
    duplicate.fileSize = 10;
    duplicate.categoryId = QString();
    QString error;
    QVERIFY2(setup.repository.insert(duplicate, &error), qPrintable(error));
    QCOMPARE(setup.repository.countByRelativePath(relative), 2);

    const QString firstId = setup.repository.list({}).first().id;
    QVERIFY(setup.service.deleteSnippet(firstId));
    QVERIFY(QFile::exists(absolute)); // still referenced

    const QString lastId = setup.repository.list({}).first().id;
    QVERIFY(setup.service.deleteSnippet(lastId));
    QVERIFY(!QFile::exists(absolute)); // last reference gone -> file removed
}

void TestSnippetService::deleteSnippetWarnsWhenCleanupFails()
{
    Setup setup;
    QVERIFY(setup.env.init());

    const QString relative = setup.placeAttachmentFile(QStringLiteral("locked.png"));
    const QString absolute = setup.attachments.absolutePath(relative);
    QVERIFY(setup.service.createAttachmentSnippet(QStringLiteral("image"),
                                                  QStringLiteral("locked.png"), relative,
                                                  QStringLiteral("image/png"), 10, QString()));
    const QString id = setup.repository.list({}).first().id;

    // Make the file undeletable (read-only attribute fails QFile::remove).
    QVERIFY(QFile::setPermissions(absolute, QFileDevice::ReadOwner));
    QSignalSpy warnings(&setup.service, &talkbox::SnippetService::warningOccurred);

    QVERIFY2(setup.service.deleteSnippet(id), qPrintable(setup.service.lastError()));
    QCOMPARE(setup.repository.list({}).size(), 0); // record is gone regardless
    QCOMPARE(warnings.count(), 1);

    // Cleanup for the test environment.
    QFile::setPermissions(absolute, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QFile::remove(absolute);
}

QTEST_GUILESS_MAIN(TestSnippetService)

#include "tst_snippet_service.moc"
