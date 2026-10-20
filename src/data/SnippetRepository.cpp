#include "data/SnippetRepository.h"

#include "data/DatabaseManager.h"

#include <QDateTime>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace talkbox {

namespace {

constexpr auto kSelectColumns =
    "SELECT s.id, s.type, s.content, s.category_id, c.name, c.color_key, "
    "s.display_name, s.relative_path, s.mime_type, s.file_size, "
    "s.created_at, s.updated_at, s.last_used_at, s.sort_order "
    "FROM snippets s LEFT JOIN categories c ON c.id = s.category_id";

QVariant nullIfEmpty(const QString &value)
{
    return value.isEmpty() ? QVariant() : QVariant(value);
}

} // namespace

SnippetRepository::SnippetRepository(DatabaseManager &database)
    : m_database(database)
{
}

bool SnippetRepository::isKnownType(const QString &type)
{
    return type == QLatin1String("text") || type == QLatin1String("image")
        || type == QLatin1String("pdf");
}

QString SnippetRepository::newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString SnippetRepository::utcNowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

Snippet SnippetRepository::snippetFromQuery(const QSqlQuery &query)
{
    Snippet snippet;
    snippet.id = query.value(0).toString();
    snippet.type = query.value(1).toString();
    snippet.content = query.value(2).toString();
    snippet.categoryId = query.value(3).toString();
    snippet.categoryName = query.value(4).toString();
    snippet.categoryColorKey = query.value(5).toString();
    snippet.displayName = query.value(6).toString();
    snippet.relativePath = query.value(7).toString();
    snippet.mimeType = query.value(8).toString();
    snippet.fileSize = query.value(9).isNull() ? -1 : query.value(9).toLongLong();
    snippet.createdAt = query.value(10).toString();
    snippet.updatedAt = query.value(11).toString();
    snippet.lastUsedAt = query.value(12).toString();
    snippet.sortOrder = query.value(13).toInt();
    return snippet;
}

std::optional<Snippet> SnippetRepository::findById(const QString &id, QString *errorMessage) const
{
    QSqlQuery query(m_database.database());
    const QString sql = QLatin1String(kSelectColumns) + QLatin1String(" WHERE s.id = ?");
    query.prepare(sql);
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return snippetFromQuery(query);
}

bool SnippetRepository::insert(Snippet &snippet, QString *errorMessage)
{
    if (!isKnownType(snippet.type)) {
        if (errorMessage)
            *errorMessage = QObject::tr("未知的内容类型：%1").arg(snippet.type);
        return false;
    }

    if (snippet.id.isEmpty())
        snippet.id = newId();
    const QString now = utcNowIso();
    if (snippet.createdAt.isEmpty())
        snippet.createdAt = now;
    if (snippet.updatedAt.isEmpty())
        snippet.updatedAt = now;

    QSqlDatabase db = m_database.database();
    if (!db.transaction()) {
        if (errorMessage)
            *errorMessage = db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO snippets (id, type, content, category_id, display_name, "
        "relative_path, mime_type, file_size, created_at, updated_at, "
        "last_used_at, sort_order) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(snippet.id);
    query.addBindValue(snippet.type);
    query.addBindValue(nullIfEmpty(snippet.content));
    query.addBindValue(nullIfEmpty(snippet.categoryId));
    query.addBindValue(nullIfEmpty(snippet.displayName));
    query.addBindValue(nullIfEmpty(snippet.relativePath));
    query.addBindValue(nullIfEmpty(snippet.mimeType));
    query.addBindValue(snippet.fileSize >= 0 ? QVariant(snippet.fileSize) : QVariant());
    query.addBindValue(snippet.createdAt);
    query.addBindValue(snippet.updatedAt);
    query.addBindValue(nullIfEmpty(snippet.lastUsedAt));
    query.addBindValue(snippet.sortOrder);

    if (!query.exec()) {
        const QString detail = query.lastError().text();
        db.rollback();
        if (errorMessage)
            *errorMessage = QObject::tr("保存内容失败：%1").arg(detail);
        return false;
    }

    if (!db.commit()) {
        const QString detail = db.lastError().text();
        db.rollback();
        if (errorMessage)
            *errorMessage = QObject::tr("保存内容失败：%1").arg(detail);
        return false;
    }

    return true;
}

bool SnippetRepository::update(const Snippet &snippet, QString *errorMessage)
{
    QSqlDatabase db = m_database.database();
    if (!db.transaction()) {
        if (errorMessage)
            *errorMessage = db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE snippets SET content = ?, category_id = ?, display_name = ?, "
        "relative_path = ?, mime_type = ?, file_size = ?, updated_at = ? "
        "WHERE id = ?"));
    query.addBindValue(nullIfEmpty(snippet.content));
    query.addBindValue(nullIfEmpty(snippet.categoryId));
    query.addBindValue(nullIfEmpty(snippet.displayName));
    query.addBindValue(nullIfEmpty(snippet.relativePath));
    query.addBindValue(nullIfEmpty(snippet.mimeType));
    query.addBindValue(snippet.fileSize >= 0 ? QVariant(snippet.fileSize) : QVariant());
    query.addBindValue(utcNowIso());
    query.addBindValue(snippet.id);

    if (!query.exec()) {
        const QString detail = query.lastError().text();
        db.rollback();
        if (errorMessage)
            *errorMessage = QObject::tr("更新内容失败：%1").arg(detail);
        return false;
    }
    if (query.numRowsAffected() == 0) {
        db.rollback();
        if (errorMessage)
            *errorMessage = QObject::tr("未找到要更新的内容。");
        return false;
    }

    if (!db.commit()) {
        const QString detail = db.lastError().text();
        db.rollback();
        if (errorMessage)
            *errorMessage = QObject::tr("更新内容失败：%1").arg(detail);
        return false;
    }

    return true;
}

bool SnippetRepository::remove(const QString &id, QString *errorMessage)
{
    QSqlDatabase db = m_database.database();
    if (!db.transaction()) {
        if (errorMessage)
            *errorMessage = db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM snippets WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        const QString detail = query.lastError().text();
        db.rollback();
        if (errorMessage)
            *errorMessage = QObject::tr("删除内容失败：%1").arg(detail);
        return false;
    }
    if (query.numRowsAffected() == 0) {
        db.rollback();
        if (errorMessage)
            *errorMessage = QObject::tr("未找到要删除的内容。");
        return false;
    }

    if (!db.commit()) {
        const QString detail = db.lastError().text();
        db.rollback();
        if (errorMessage)
            *errorMessage = QObject::tr("删除内容失败：%1").arg(detail);
        return false;
    }

    return true;
}

QList<Snippet> SnippetRepository::list(const SnippetFilter &filter, QString *errorMessage) const
{
    QList<Snippet> result;

    QStringList conditions;
    QVariantList binds;

    const QString search = filter.searchQuery.trimmed();
    if (!search.isEmpty()) {
        conditions.append(QStringLiteral(
            "(s.content LIKE ? ESCAPE '\\' OR s.display_name LIKE ? ESCAPE '\\' "
            "OR c.name LIKE ? ESCAPE '\\')"));
        const QString pattern = QLatin1Char('%') + escapeLike(search) + QLatin1Char('%');
        binds << pattern << pattern << pattern;
    }

    const QString categoryId = filter.categoryId.trimmed();
    if (categoryId == SnippetFilter::uncategorizedKey()) {
        conditions.append(QStringLiteral("s.category_id IS NULL"));
    } else if (!categoryId.isEmpty() && categoryId != QLatin1String("all")) {
        conditions.append(QStringLiteral("s.category_id = ?"));
        binds << categoryId;
    }

    const QString type = filter.type.trimmed();
    if (!type.isEmpty() && type != QLatin1String("all")) {
        conditions.append(QStringLiteral("s.type = ?"));
        binds << type;
    }

    QString sql = QLatin1String(kSelectColumns);
    if (!conditions.isEmpty())
        sql += QLatin1String(" WHERE ") + conditions.join(QLatin1String(" AND "));
    sql += QLatin1String(" ORDER BY s.updated_at DESC, s.created_at DESC, s.id ASC");

    QSqlQuery query(m_database.database());
    query.prepare(sql);
    for (const QVariant &bind : binds)
        query.addBindValue(bind);

    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return result;
    }

    while (query.next())
        result.append(snippetFromQuery(query));
    return result;
}

bool SnippetRepository::touchLastUsed(const QString &id, QString *errorMessage)
{
    QSqlQuery query(m_database.database());
    query.prepare(QStringLiteral("UPDATE snippets SET last_used_at = ? WHERE id = ?"));
    query.addBindValue(utcNowIso());
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

int SnippetRepository::countByRelativePath(const QString &relativePath, QString *errorMessage) const
{
    QSqlQuery query(m_database.database());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM snippets WHERE relative_path = ?"));
    query.addBindValue(relativePath);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return 0;
    }
    return query.next() ? query.value(0).toInt() : 0;
}

QString SnippetRepository::escapeLike(const QString &term)
{
    QString escaped;
    escaped.reserve(term.size());
    for (const QChar c : term) {
        if (c == QLatin1Char('\\') || c == QLatin1Char('%') || c == QLatin1Char('_'))
            escaped.append(QLatin1Char('\\'));
        escaped.append(c);
    }
    return escaped;
}

} // namespace talkbox
