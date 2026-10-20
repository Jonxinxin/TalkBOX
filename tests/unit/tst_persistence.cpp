#include "TestUtils.h"

#include "data/CategoryRepository.h"
#include "data/Migrations.h"
#include "data/SnippetRepository.h"

#include <QSqlQuery>
#include <QTest>
#include <QVariant>

/// Simulates an application restart: close the database manager, reopen
/// the same file and verify that schema version, categories and content
/// are still intact (acceptance AT-02).
class TestPersistence : public QObject
{
    Q_OBJECT

private slots:
    void dataSurvivesReopen();
    void schemaVersionStableAcrossReopen();
};

void TestPersistence::dataSurvivesReopen()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    QString snippetId;
    QString categoryId;
    {
        talkbox::CategoryRepository categories(env.database);
        talkbox::Category category;
        QVERIFY2(categories.create(QStringLiteral("报价"), QString(), &category, &error),
                 qPrintable(error));
        categoryId = category.id;
    }
    {
        talkbox::SnippetRepository repository(env.database);
        talkbox::Snippet snippet;
        snippet.type = QStringLiteral("text");
        snippet.content = QStringLiteral("重启后仍应存在的话术");
        snippet.categoryId = categoryId;
        QVERIFY2(repository.insert(snippet, &error), qPrintable(error));
        snippetId = snippet.id;
    }

    // "Restart": separate connection, same database file.
    {
        talkbox::DatabaseManager reopened(testutils::uniqueConnectionName());
        QVERIFY2(reopened.open(env.databaseFilePath(), &error), qPrintable(error));

        talkbox::SnippetRepository repository(reopened);
        const auto found = repository.findById(snippetId, &error);
        QVERIFY2(found.has_value(), qPrintable(error));
        QCOMPARE(found->content, QStringLiteral("重启后仍应存在的话术"));
        QCOMPARE(found->categoryName, QStringLiteral("报价"));
        QCOMPARE(repository.list(talkbox::SnippetFilter{}).size(), 1);

        // The user category is still there after the restart.
        talkbox::CategoryRepository categories(reopened);
        QCOMPARE(categories.listAll(&error).size(), 1);
    }
}

void TestPersistence::schemaVersionStableAcrossReopen()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    for (int cycle = 0; cycle < 2; ++cycle) {
        talkbox::DatabaseManager reopened(testutils::uniqueConnectionName());
        QVERIFY2(reopened.open(env.databaseFilePath(), &error), qPrintable(error));

        // No built-in categories are seeded and nothing gets duplicated.
        talkbox::CategoryRepository categories(reopened);
        QCOMPARE(categories.listAll().size(), 0);

        QSqlQuery query(reopened.database());
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), talkbox::migrations::kTargetVersion);
    }
}

QTEST_GUILESS_MAIN(TestPersistence)

#include "tst_persistence.moc"
