#include "TestUtils.h"

#include "data/CategoryRepository.h"
#include "data/SnippetRepository.h"

#include <QTest>

using talkbox::Category;
using talkbox::CategoryRepository;
using talkbox::Snippet;
using talkbox::SnippetFilter;
using talkbox::SnippetRepository;

namespace {

Snippet makeText(const QString &content, const QString &categoryId)
{
    Snippet snippet;
    snippet.type = QStringLiteral("text");
    snippet.content = content;
    snippet.categoryId = categoryId;
    return snippet;
}

Snippet makeImage(const QString &displayName, const QString &relativePath,
                  const QString &categoryId)
{
    Snippet snippet;
    snippet.type = QStringLiteral("image");
    snippet.displayName = displayName;
    snippet.relativePath = relativePath;
    snippet.categoryId = categoryId;
    return snippet;
}

/// Every category is user created; tests make the ones they need.
QString createCategory(testutils::TestEnv &env, const QString &name)
{
    CategoryRepository repository(env.database);
    Category created;
    return repository.create(name, QString(), &created) ? created.id : QString();
}

} // namespace

class TestSnippetRepository : public QObject
{
    Q_OBJECT

private slots:
    void insertsAndFindsTextSnippet();
    void listOrdersByMostRecentUpdate();
    void searchMatchesContentDisplayNameAndCategory();
    void filtersCombine();
    void uncategorizedFilterMatchesMissingCategory();
    void wildcardsAreEscaped();
    void updateChangesContentAndUpdatedAt();
    void removeDeletesRow();
    void touchLastUsedSetsTimestampWithoutTouchingUpdatedAt();
    void rejectsUnknownType();
    void countsReferencesByRelativePath();
};

void TestSnippetRepository::insertsAndFindsTextSnippet()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    const QString opening = createCategory(env, QStringLiteral("开场"));
    QVERIFY(!opening.isEmpty());

    SnippetRepository repository(env.database);
    Snippet snippet = makeText(QStringLiteral("你好！\n欢迎咨询充电桩安装。\n可以随时联系我。"),
                               opening);
    QVERIFY2(repository.insert(snippet, &error), qPrintable(error));
    QVERIFY(!snippet.id.isEmpty());
    QVERIFY(!snippet.createdAt.isEmpty());
    QVERIFY(!snippet.updatedAt.isEmpty());

    const auto found = repository.findById(snippet.id, &error);
    QVERIFY2(found.has_value(), qPrintable(error));
    QCOMPARE(found->content, snippet.content); // multiline content is preserved
    QCOMPARE(found->type, QStringLiteral("text"));
    QCOMPARE(found->categoryId, opening);
    QCOMPARE(found->categoryName, QStringLiteral("开场")); // join works
    QCOMPARE(found->categoryColorKey, QStringLiteral("violet"));
    QVERIFY(found->isText());

    const auto missing = repository.findById(QStringLiteral("no-such-id"), &error);
    QVERIFY(!missing.has_value());
}

void TestSnippetRepository::listOrdersByMostRecentUpdate()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    SnippetRepository repository(env.database);
    Snippet first = makeText(QStringLiteral("第一条"), QString());
    QVERIFY2(repository.insert(first, &error), qPrintable(error));
    QTest::qSleep(5);

    Snippet second = makeText(QStringLiteral("第二条"), QString());
    QVERIFY2(repository.insert(second, &error), qPrintable(error));

    QList<Snippet> all = repository.list(SnippetFilter{}, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(all.size(), 2);
    QCOMPARE(all.at(0).id, second.id); // newest update first

    QTest::qSleep(5);
    first.content = QStringLiteral("第一条（已更新）");
    QVERIFY2(repository.update(first, &error), qPrintable(error));

    all = repository.list(SnippetFilter{}, &error);
    QCOMPARE(all.size(), 2);
    QCOMPARE(all.at(0).id, first.id); // updated entry moves to the front
}

void TestSnippetRepository::searchMatchesContentDisplayNameAndCategory()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    const QString quotes = createCategory(env, QStringLiteral("报价"));
    const QString aftersale = createCategory(env, QStringLiteral("售后"));
    QVERIFY(!quotes.isEmpty());
    QVERIFY(!aftersale.isEmpty());

    SnippetRepository repository(env.database);
    Snippet text1 = makeText(QStringLiteral("家用充电桩的安装流程说明"), quotes);
    Snippet text2 = makeText(QStringLiteral("价格可以再谈"), aftersale);
    Snippet image1 = makeImage(QStringLiteral("门店海报.png"),
                              QStringLiteral("attachments/a.png"), quotes);
    QVERIFY(repository.insert(text1, &error));
    QVERIFY(repository.insert(text2, &error));
    QVERIFY(repository.insert(image1, &error));

    SnippetFilter filter;
    filter.searchQuery = QStringLiteral("安装流程");
    QCOMPARE(repository.list(filter, &error).size(), 1);

    filter.searchQuery = QStringLiteral("海报"); // display name of the image
    QCOMPARE(repository.list(filter, &error).size(), 1);

    filter.searchQuery = QStringLiteral("售后"); // category name
    const QList<Snippet> byCategory = repository.list(filter, &error);
    QCOMPARE(byCategory.size(), 1);
    QCOMPARE(byCategory.first().id, text2.id);

    filter.searchQuery = QStringLiteral("不存在的关键词");
    QCOMPARE(repository.list(filter, &error).size(), 0);
}

void TestSnippetRepository::filtersCombine()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    const QString quotes = createCategory(env, QStringLiteral("报价"));
    const QString aftersale = createCategory(env, QStringLiteral("售后"));
    QVERIFY(!quotes.isEmpty());
    QVERIFY(!aftersale.isEmpty());

    SnippetRepository repository(env.database);
    Snippet text1 = makeText(QStringLiteral("家用充电桩的安装流程说明"), quotes);
    Snippet text2 = makeText(QStringLiteral("价格可以再谈"), aftersale);
    Snippet image1 = makeImage(QStringLiteral("门店海报.png"),
                              QStringLiteral("attachments/a.png"), quotes);
    QVERIFY(repository.insert(text1, &error));
    QVERIFY(repository.insert(text2, &error));
    QVERIFY(repository.insert(image1, &error));

    SnippetFilter filter;
    QCOMPARE(repository.list(filter, &error).size(), 3);

    filter.categoryId = quotes;
    QCOMPARE(repository.list(filter, &error).size(), 2);

    filter.type = QStringLiteral("image");
    QCOMPARE(repository.list(filter, &error).size(), 1);

    filter.type = QStringLiteral("text");
    QCOMPARE(repository.list(filter, &error).size(), 1);

    filter.categoryId = aftersale;
    QCOMPARE(repository.list(filter, &error).size(), 1);

    filter.categoryId = QStringLiteral("all");
    filter.type = QStringLiteral("all");
    filter.searchQuery = QStringLiteral("价格");
    QCOMPARE(repository.list(filter, &error).size(), 1);

    filter.categoryId = quotes;
    QCOMPARE(repository.list(filter, &error).size(), 0); // "价格" is not in 报价 entries

    filter.categoryId = QStringLiteral("all");
    filter.searchQuery = QString();
    filter.type = QStringLiteral("pdf");
    QCOMPARE(repository.list(filter, &error).size(), 0);
}

void TestSnippetRepository::uncategorizedFilterMatchesMissingCategory()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    const QString quotes = createCategory(env, QStringLiteral("报价"));
    SnippetRepository repository(env.database);
    Snippet categorized = makeText(QStringLiteral("有分类"), quotes);
    Snippet uncategorized = makeText(QStringLiteral("无分类"), QString());
    QVERIFY2(repository.insert(categorized, &error), qPrintable(error));
    QVERIFY2(repository.insert(uncategorized, &error), qPrintable(error));

    SnippetFilter filter;
    filter.categoryId = SnippetFilter::uncategorizedKey();
    const QList<Snippet> result = repository.list(filter, &error);
    QCOMPARE(result.size(), 1);
    QCOMPARE(result.first().content, QStringLiteral("无分类"));
    QVERIFY(result.first().categoryId.isEmpty());
    QVERIFY(result.first().categoryName.isEmpty());
}

void TestSnippetRepository::wildcardsAreEscaped()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    SnippetRepository repository(env.database);
    Snippet percent = makeText(QStringLiteral("100% 纯铜线"), QString());
    Snippet number = makeText(QStringLiteral("1000 元以上"), QString());
    Snippet underscore = makeText(QStringLiteral("A_B 型号"), QString());
    Snippet plain = makeText(QStringLiteral("AXB 型号"), QString());
    QVERIFY(repository.insert(percent, &error));
    QVERIFY(repository.insert(number, &error));
    QVERIFY(repository.insert(underscore, &error));
    QVERIFY(repository.insert(plain, &error));

    SnippetFilter filter;
    filter.searchQuery = QStringLiteral("100%");
    QList<Snippet> results = repository.list(filter, &error);
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().id, percent.id);

    filter.searchQuery = QStringLiteral("A_B");
    results = repository.list(filter, &error);
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().id, underscore.id);

    filter.searchQuery = QStringLiteral("%");
    results = repository.list(filter, &error);
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().id, percent.id);
}

void TestSnippetRepository::updateChangesContentAndUpdatedAt()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    const QString meters = createCategory(env, QStringLiteral("电表"));
    QVERIFY(!meters.isEmpty());

    SnippetRepository repository(env.database);
    Snippet snippet = makeText(QStringLiteral("初始内容"), QString());
    QVERIFY2(repository.insert(snippet, &error), qPrintable(error));
    QTest::qSleep(5);

    auto loaded = repository.findById(snippet.id, &error);
    QVERIFY(loaded.has_value());
    const QString originalCreatedAt = loaded->createdAt;
    const QString originalUpdatedAt = loaded->updatedAt;

    loaded->content = QStringLiteral("更新后的内容");
    loaded->categoryId = meters;
    QVERIFY2(repository.update(*loaded, &error), qPrintable(error));

    const auto after = repository.findById(snippet.id, &error);
    QVERIFY(after.has_value());
    QCOMPARE(after->content, QStringLiteral("更新后的内容"));
    QCOMPARE(after->categoryId, meters);
    QCOMPARE(after->categoryName, QStringLiteral("电表"));
    QCOMPARE(after->createdAt, originalCreatedAt); // creation time unchanged
    QVERIFY(after->updatedAt > originalUpdatedAt); // update time advanced

    Snippet unknown;
    unknown.id = QStringLiteral("missing-id");
    unknown.type = QStringLiteral("text");
    unknown.content = QStringLiteral("x");
    QVERIFY(!repository.update(unknown, &error));
    QVERIFY(!error.isEmpty());
}

void TestSnippetRepository::removeDeletesRow()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    SnippetRepository repository(env.database);
    Snippet snippet = makeText(QStringLiteral("将被删除"), QString());
    QVERIFY2(repository.insert(snippet, &error), qPrintable(error));

    QVERIFY2(repository.remove(snippet.id, &error), qPrintable(error));
    QVERIFY(!repository.findById(snippet.id, &error).has_value());

    QVERIFY(!repository.remove(snippet.id, &error)); // second delete fails visibly
    QVERIFY(!error.isEmpty());
}

void TestSnippetRepository::touchLastUsedSetsTimestampWithoutTouchingUpdatedAt()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    SnippetRepository repository(env.database);
    Snippet snippet = makeText(QStringLiteral("使用记录测试"), QString());
    QVERIFY2(repository.insert(snippet, &error), qPrintable(error));

    const auto before = repository.findById(snippet.id, &error);
    QVERIFY(before.has_value());
    QVERIFY(before->lastUsedAt.isEmpty());

    QVERIFY2(repository.touchLastUsed(snippet.id, &error), qPrintable(error));

    const auto after = repository.findById(snippet.id, &error);
    QVERIFY(after.has_value());
    QVERIFY(!after->lastUsedAt.isEmpty());
    QCOMPARE(after->updatedAt, before->updatedAt); // sort order must not change
}

void TestSnippetRepository::rejectsUnknownType()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    SnippetRepository repository(env.database);
    Snippet snippet;
    snippet.type = QStringLiteral("video");
    snippet.content = QStringLiteral("x");

    QVERIFY(!repository.insert(snippet, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(repository.list(SnippetFilter{}).size(), 0);
}

void TestSnippetRepository::countsReferencesByRelativePath()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    const QString quotes = createCategory(env, QStringLiteral("报价"));
    SnippetRepository repository(env.database);
    const QString path = QStringLiteral("attachments/ref-test.png");

    Snippet first = makeImage(QStringLiteral("first.png"), path, quotes);
    Snippet second = makeImage(QStringLiteral("second.png"), path, quotes);
    QVERIFY2(repository.insert(first, &error), qPrintable(error));
    QVERIFY2(repository.insert(second, &error), qPrintable(error));

    QCOMPARE(repository.countByRelativePath(path, &error), 2);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(repository.countByRelativePath(QStringLiteral("attachments/other.png"), &error), 0);

    QVERIFY(repository.remove(first.id, &error));
    QCOMPARE(repository.countByRelativePath(path, &error), 1);
}

QTEST_GUILESS_MAIN(TestSnippetRepository)

#include "tst_snippet_repository.moc"
