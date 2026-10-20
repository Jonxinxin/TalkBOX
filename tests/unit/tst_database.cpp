#include "TestUtils.h"

#include "data/CategoryRepository.h"
#include "data/Migrations.h"
#include "data/SnippetRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTest>
#include <QVariant>

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void createsSchemaWithoutBuiltinCategories();
    void migrationIsIdempotent();
    void foreignKeyIsEnforced();
    void oldBuiltinCategoriesAreDroppedOnUpgrade();
};

void TestDatabase::createsSchemaWithoutBuiltinCategories()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    QSqlQuery query(env.database.database());
    QVERIFY2(query.exec(QStringLiteral(
                 "SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name")),
             qPrintable(query.lastError().text()));

    QStringList tables;
    while (query.next())
        tables << query.value(0).toString();
    QVERIFY(tables.contains(QStringLiteral("snippets")));
    QVERIFY(tables.contains(QStringLiteral("categories")));
    QVERIFY(tables.contains(QStringLiteral("app_settings")));

    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), talkbox::migrations::kTargetVersion);

    // No built-in categories: the list starts empty.
    talkbox::CategoryRepository categories(env.database);
    const QList<talkbox::Category> all = categories.listAll(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(all.size(), 0);

    // The user creates what they need.
    QVERIFY(categories.create(QStringLiteral("开场"), QString(), nullptr, &error));
    QCOMPARE(categories.listAll(&error).size(), 1);
}

void TestDatabase::migrationIsIdempotent()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    talkbox::CategoryRepository categories(env.database);
    QVERIFY(categories.create(QStringLiteral("自定义"), QString(), nullptr, &error));

    // A second manager on the same file must not re-run the migration,
    // duplicate categories or change the schema version.
    {
        talkbox::DatabaseManager second(testutils::uniqueConnectionName());
        QVERIFY2(second.open(env.databaseFilePath(), &error), qPrintable(error));
    }

    QCOMPARE(categories.listAll().size(), 1);

    QSqlQuery query(env.database.database());
    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), talkbox::migrations::kTargetVersion);
}

void TestDatabase::foreignKeyIsEnforced()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    QSqlQuery query(env.database.database());
    query.prepare(QStringLiteral(
        "INSERT INTO snippets (id, type, content, category_id, created_at, updated_at) "
        "VALUES ('fk-test', 'text', 'hello', 'missing-category', '2026-01-01T00:00:00.000Z', "
        "'2026-01-01T00:00:00.000Z')"));
    QVERIFY2(!query.exec(), "Insert with an unknown category id must fail");

    // A snippet without a category is valid ("未分类").
    query.prepare(QStringLiteral(
        "INSERT INTO snippets (id, type, content, category_id, created_at, updated_at) "
        "VALUES ('no-category', 'text', 'hello', NULL, '2026-01-01T00:00:00.000Z', "
        "'2026-01-01T00:00:00.000Z')"));
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}

void TestDatabase::oldBuiltinCategoriesAreDroppedOnUpgrade()
{
    testutils::TestEnv env;
    QString error;
    QVERIFY2(env.init(&error), qPrintable(error));

    // Simulate a pre-v3 database: seeded built-in rows, one user category and
    // snippets assigned to both.
    QSqlQuery query(env.database.database());
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO categories (id, name, color_key, sort_order, created_at) VALUES "
        "('builtin-opening', '开场', 'blue', 10, '2026-01-01T00:00:00.000Z'),"
        "('builtin-quotes', '报价', 'amber', 30, '2026-01-01T00:00:00.000Z'),"
        "('user-own', '自己的分类', 'violet', 40, '2026-01-01T00:00:00.000Z')")));
    // The real v1 seed rows are built-ins; mark them accordingly.
    QVERIFY(query.exec(QStringLiteral(
        "UPDATE categories SET is_builtin = 1 WHERE id IN ('builtin-opening', 'builtin-quotes')")));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO snippets (id, type, content, category_id, created_at, updated_at) VALUES "
        "('old-builtin', 'text', '旧内置分类内容', 'builtin-quotes', "
        " '2026-01-01T00:00:00.000Z', '2026-01-01T00:00:00.000Z'),"
        "('old-user', 'text', '旧自定义分类内容', 'user-own', "
        " '2026-01-01T00:00:00.000Z', '2026-01-01T00:00:00.000Z')")));
    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 2")));

    // Reopening replays the v3 migration.
    {
        talkbox::DatabaseManager migrated(testutils::uniqueConnectionName());
        QVERIFY2(migrated.open(env.databaseFilePath(), &error), qPrintable(error));

        talkbox::CategoryRepository categories(migrated);
        const QList<talkbox::Category> remaining = categories.listAll(&error);
        QCOMPARE(remaining.size(), 1);
        QCOMPARE(remaining.first().id, QStringLiteral("user-own"));

        talkbox::SnippetRepository snippets(migrated);
        QVERIFY(snippets.findById(QStringLiteral("old-builtin"))->categoryId.isEmpty());
        QCOMPARE(snippets.findById(QStringLiteral("old-user"))->categoryId,
                 QStringLiteral("user-own"));
    }

    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), talkbox::migrations::kTargetVersion);
}

QTEST_GUILESS_MAIN(TestDatabase)

#include "tst_database.moc"
