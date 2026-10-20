#include "data/Migrations.h"

#include <QDateTime>
#include <QDebug>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace talkbox::migrations {

namespace {

struct Migration
{
    int version;
    QStringList statements;
};

QStringList version1Statements()
{
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    return QStringList{
        QStringLiteral(R"SQL(
            CREATE TABLE categories (
                id         TEXT PRIMARY KEY,
                name       TEXT NOT NULL UNIQUE,
                color_key  TEXT NOT NULL,
                is_builtin INTEGER NOT NULL DEFAULT 0,
                sort_order INTEGER NOT NULL DEFAULT 0,
                created_at TEXT NOT NULL
            )
        )SQL"),
        QStringLiteral(R"SQL(
            CREATE TABLE snippets (
                id            TEXT PRIMARY KEY,
                type          TEXT NOT NULL CHECK (type IN ('text','image','pdf')),
                content       TEXT,
                category_id   TEXT NOT NULL REFERENCES categories(id) ON DELETE RESTRICT,
                display_name  TEXT,
                relative_path TEXT,
                mime_type     TEXT,
                file_size     INTEGER,
                created_at    TEXT NOT NULL,
                updated_at    TEXT NOT NULL,
                last_used_at  TEXT,
                sort_order    INTEGER NOT NULL DEFAULT 0,
                CHECK (
                    (type = 'text' AND content IS NOT NULL)
                    OR (type IN ('image','pdf') AND relative_path IS NOT NULL)
                )
            )
        )SQL"),
        QStringLiteral(R"SQL(
            CREATE TABLE app_settings (
                key   TEXT PRIMARY KEY,
                value TEXT NOT NULL
            )
        )SQL"),
        QStringLiteral(
            "CREATE INDEX idx_snippets_category ON snippets(category_id)"),
        QStringLiteral(
            "CREATE INDEX idx_snippets_type ON snippets(type)"),
        QStringLiteral(
            "CREATE INDEX idx_snippets_updated_at ON snippets(updated_at DESC)"),
        QStringLiteral(
            "CREATE INDEX idx_snippets_last_used_at ON snippets(last_used_at DESC)"),
        // Built-in categories. "全部" is a virtual filter and is not stored.
        QStringLiteral(R"SQL(
            INSERT INTO categories (id, name, color_key, is_builtin, sort_order, created_at) VALUES
                ('builtin-opening',       '开场',  'blue',  1, 10, '%1'),
                ('builtin-meters',        '电表',  'teal',  1, 20, '%1'),
                ('builtin-quotes',        '报价',  'amber', 1, 30, '%1'),
                ('builtin-aftersale',     '售后',  'rose',  1, 40, '%1'),
                ('builtin-uncategorized', '未分类','slate', 1, 90, '%1')
        )SQL")
            .arg(now),
    };
}

// v2: "未分类" moves to the end of the list with a large sort order so
// user-defined categories (phase C) can slot in before it.
QStringList version2Statements()
{
    return QStringList{
        QStringLiteral(
            "UPDATE categories SET sort_order = 1000 WHERE id = 'builtin-uncategorized'"),
    };
}

// v3: no built-in categories any more. The user decides which categories
// exist, so the seeded rows are removed and a snippet may now live without a
// category (shown as "未分类" by the UI, never stored as a fake category).
// SQLite cannot drop NOT NULL on an existing column, hence the table rebuild.
QStringList version3Statements()
{
    return QStringList{
        QStringLiteral(R"SQL(
            CREATE TABLE snippets_new (
                id            TEXT PRIMARY KEY,
                type          TEXT NOT NULL CHECK (type IN ('text','image','pdf')),
                content       TEXT,
                category_id   TEXT REFERENCES categories(id) ON DELETE SET NULL,
                display_name  TEXT,
                relative_path TEXT,
                mime_type     TEXT,
                file_size     INTEGER,
                created_at    TEXT NOT NULL,
                updated_at    TEXT NOT NULL,
                last_used_at  TEXT,
                sort_order    INTEGER NOT NULL DEFAULT 0,
                CHECK (
                    (type = 'text' AND content IS NOT NULL)
                    OR (type IN ('image','pdf') AND relative_path IS NOT NULL)
                )
            )
        )SQL"),
        QStringLiteral(R"SQL(
            INSERT INTO snippets_new (id, type, content, category_id, display_name,
                                      relative_path, mime_type, file_size,
                                      created_at, updated_at, last_used_at, sort_order)
            SELECT id, type, content,
                   CASE WHEN category_id IN (SELECT id FROM categories WHERE is_builtin = 1)
                        THEN NULL ELSE category_id END,
                   display_name, relative_path, mime_type, file_size,
                   created_at, updated_at, last_used_at, sort_order
            FROM snippets
        )SQL"),
        QStringLiteral("DROP TABLE snippets"),
        QStringLiteral("ALTER TABLE snippets_new RENAME TO snippets"),
        QStringLiteral("DELETE FROM categories WHERE is_builtin = 1"),
        QStringLiteral("CREATE INDEX idx_snippets_category ON snippets(category_id)"),
        QStringLiteral("CREATE INDEX idx_snippets_type ON snippets(type)"),
        QStringLiteral(
            "CREATE INDEX idx_snippets_updated_at ON snippets(updated_at DESC)"),
        QStringLiteral(
            "CREATE INDEX idx_snippets_last_used_at ON snippets(last_used_at DESC)"),
    };
}

const QList<Migration> &migrationList()
{
    static const QList<Migration> migrations{
        Migration{1, version1Statements()},
        Migration{2, version2Statements()},
        Migration{3, version3Statements()},
    };
    return migrations;
}

int readUserVersion(QSqlDatabase &database)
{
    QSqlQuery query(database);
    if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next())
        return query.value(0).toInt();
    return 0;
}

} // namespace

bool apply(QSqlDatabase &database, QString *errorMessage)
{
    int currentVersion = readUserVersion(database);
    if (currentVersion > kTargetVersion) {
        if (errorMessage) {
            *errorMessage = QObject::tr(
                "数据库版本(%1)高于当前软件支持的版本(%2)，请升级 TalkBOX 或恢复备份。")
                .arg(currentVersion)
                .arg(kTargetVersion);
        }
        return false;
    }

    for (const Migration &migration : migrationList()) {
        if (migration.version <= currentVersion)
            continue;

        if (!database.transaction()) {
            if (errorMessage)
                *errorMessage = database.lastError().text();
            return false;
        }

        QSqlQuery query(database);
        for (const QString &statement : migration.statements) {
            if (!query.exec(statement)) {
                const QString detail = query.lastError().text();
                database.rollback();
                if (errorMessage) {
                    *errorMessage = QObject::tr("数据库迁移(v%1)失败：%2")
                                        .arg(migration.version)
                                        .arg(detail);
                }
                return false;
            }
        }

        if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(migration.version))) {
            const QString detail = query.lastError().text();
            database.rollback();
            if (errorMessage) {
                *errorMessage = QObject::tr("无法写入数据库版本号：%1").arg(detail);
            }
            return false;
        }

        if (!database.commit()) {
            const QString detail = database.lastError().text();
            database.rollback();
            if (errorMessage) {
                *errorMessage = QObject::tr("数据库迁移提交失败：%1").arg(detail);
            }
            return false;
        }

        qInfo().noquote() << "Database migration applied: v" << migration.version;
        currentVersion = migration.version;
    }

    return true;
}

} // namespace talkbox::migrations
