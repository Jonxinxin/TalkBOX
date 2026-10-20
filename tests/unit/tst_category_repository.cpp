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

} // namespace

class TestCategoryRepository : public QObject
{
    Q_OBJECT

private slots:
    void startsEmpty();
    void createsUserCategoryWithPaletteColorAndOrder();
    void createRejectsEmptyAndDuplicateNames();
    void renameWorksAndValidates();
    void deleteClearsCategoryOfItsSnippets();
    void nameExistsIgnoresCaseAndWhitespace();
    void snippetCountCountsAssignedSnippets();
};

void TestCategoryRepository::startsEmpty()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    // No built-in categories: the user decides which categories exist.
    CategoryRepository repository(env.database);
    QCOMPARE(repository.listAll(&error).size(), 0);
    QVERIFY2(error.isEmpty(), qPrintable(error));
}

void TestCategoryRepository::createsUserCategoryWithPaletteColorAndOrder()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    CategoryRepository repository(env.database);
    Category created;
    QVERIFY2(repository.create(QStringLiteral("安装指导"), QString(), &created, &error),
             qPrintable(error));
    QVERIFY(!created.id.isEmpty());
    QVERIFY(!created.colorKey.isEmpty());
    QVERIFY(CategoryRepository::paletteKeys().contains(created.colorKey));

    const QList<Category> all = repository.listAll(&error);
    QCOMPARE(all.size(), 1);
    QCOMPARE(all.at(0).id, created.id);

    // Second category gets a different palette colour and keeps the order.
    Category second;
    QVERIFY(repository.create(QStringLiteral("售后跟进"), QString(), &second, &error));
    const QList<Category> afterSecond = repository.listAll(&error);
    QCOMPARE(afterSecond.size(), 2);
    QCOMPARE(afterSecond.at(0).name, QStringLiteral("安装指导"));
    QCOMPARE(afterSecond.at(1).name, QStringLiteral("售后跟进"));
    QVERIFY(second.colorKey != created.colorKey);
    QVERIFY(second.sortOrder > created.sortOrder);
}

void TestCategoryRepository::createRejectsEmptyAndDuplicateNames()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    CategoryRepository repository(env.database);
    QVERIFY(!repository.create(QStringLiteral("   "), QString(), nullptr, &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(repository.create(QStringLiteral("应收账款"), QString(), nullptr, &error));
    QVERIFY(!repository.create(QStringLiteral("应收账款"), QString(), nullptr, &error));
    QVERIFY(!repository.create(QStringLiteral("  应收账款  "), QString(), nullptr, &error));
    QCOMPARE(repository.listAll().size(), 1);
}

void TestCategoryRepository::renameWorksAndValidates()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    CategoryRepository repository(env.database);
    Category created;
    QVERIFY(repository.create(QStringLiteral("旧名称"), QString(), &created, &error));

    QVERIFY2(repository.rename(created.id, QStringLiteral("新名称"), &error), qPrintable(error));
    QCOMPARE(repository.findById(created.id)->name, QStringLiteral("新名称"));

    QVERIFY(!repository.rename(created.id, QStringLiteral("   "), &error));
    QVERIFY(!error.isEmpty());

    Category other;
    QVERIFY(repository.create(QStringLiteral("另一个"), QString(), &other, &error));
    QVERIFY(!repository.rename(other.id, QStringLiteral("新名称"), &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(!repository.rename(QStringLiteral("missing-id"), QStringLiteral("x"), &error));
}

void TestCategoryRepository::deleteClearsCategoryOfItsSnippets()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    CategoryRepository categories(env.database);
    SnippetRepository snippets(env.database);

    Category created;
    QVERIFY(categories.create(QStringLiteral("临时分类"), QString(), &created, &error));

    Snippet first = makeText(QStringLiteral("内容一"), created.id);
    Snippet second = makeText(QStringLiteral("内容二"), created.id);
    Snippet untouched = makeText(QStringLiteral("内容三"), QString());
    QVERIFY(snippets.insert(first, &error));
    QVERIFY(snippets.insert(second, &error));
    QVERIFY(snippets.insert(untouched, &error));
    QCOMPARE(categories.snippetCount(created.id), 2);

    int moved = 0;
    QVERIFY2(categories.remove(created.id, &moved, &error), qPrintable(error));
    QCOMPARE(moved, 2);
    QVERIFY(!categories.findById(created.id).has_value());

    // The snippets survive without a category (shown as "未分类" by the UI).
    QVERIFY(snippets.findById(first.id)->categoryId.isEmpty());
    QVERIFY(snippets.findById(second.id)->categoryId.isEmpty());

    SnippetFilter filter;
    filter.categoryId = SnippetFilter::uncategorizedKey();
    QCOMPARE(snippets.list(filter, &error).size(), 3);
    QCOMPARE(snippets.list(SnippetFilter{}, &error).size(), 3);
}

void TestCategoryRepository::nameExistsIgnoresCaseAndWhitespace()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    CategoryRepository repository(env.database);
    Category quotes;
    QVERIFY(repository.create(QStringLiteral("报价"), QString(), &quotes, &error));
    QVERIFY(repository.nameExists(QStringLiteral("报价")));
    QVERIFY(repository.nameExists(QStringLiteral("  报价  ")));
    QVERIFY(!repository.nameExists(QStringLiteral("报价"), quotes.id));
    QVERIFY(!repository.nameExists(QStringLiteral("不存在的分类")));

    QVERIFY(repository.create(QStringLiteral("ABC"), QString(), nullptr, &error));
    QVERIFY(repository.nameExists(QStringLiteral("abc")));
}

void TestCategoryRepository::snippetCountCountsAssignedSnippets()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    CategoryRepository categories(env.database);
    SnippetRepository snippets(env.database);

    Category quotes;
    QVERIFY(categories.create(QStringLiteral("报价"), QString(), &quotes, &error));
    QCOMPARE(categories.snippetCount(quotes.id), 0);

    Snippet snippet = makeText(QStringLiteral("报价话术"), quotes.id);
    QVERIFY(snippets.insert(snippet, &error));
    QCOMPARE(categories.snippetCount(quotes.id), 1);
}

QTEST_GUILESS_MAIN(TestCategoryRepository)

#include "tst_category_repository.moc"
