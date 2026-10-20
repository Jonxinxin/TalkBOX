#include "data/CategoryRepository.h"

#include "data/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace talkbox {

namespace {

Category categoryFromQuery(const QSqlQuery &query)
{
    Category category;
    category.id = query.value(0).toString();
    category.name = query.value(1).toString();
    category.colorKey = query.value(2).toString();
    category.sortOrder = query.value(3).toInt();
    category.createdAt = query.value(4).toString();
    return category;
}

constexpr auto kSelectColumns =
    "SELECT id, name, color_key, sort_order, created_at FROM categories";

QString nowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

} // namespace

CategoryRepository::CategoryRepository(DatabaseManager &database)
    : m_database(database)
{
}

QStringList CategoryRepository::paletteKeys()
{
    return {QStringLiteral("violet"), QStringLiteral("teal"), QStringLiteral("blue"),
            QStringLiteral("amber"), QStringLiteral("rose"), QStringLiteral("slate")};
}

QList<Category> CategoryRepository::listAll(QString *errorMessage) const
{
    QList<Category> result;

    QSqlQuery query(m_database.database());
    const QString sql = QLatin1String(kSelectColumns)
        + QLatin1String(" ORDER BY sort_order ASC, name ASC");
    if (!query.exec(sql)) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return result;
    }

    while (query.next())
        result.append(categoryFromQuery(query));
    return result;
}

std::optional<Category> CategoryRepository::findById(const QString &id, QString *errorMessage) const
{
    QSqlQuery query(m_database.database());
    const QString sql = QLatin1String(kSelectColumns) + QLatin1String(" WHERE id = ?");
    query.prepare(sql);
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return categoryFromQuery(query);
}

bool CategoryRepository::exists(const QString &id, QString *errorMessage) const
{
    QSqlQuery query(m_database.database());
    query.prepare(QStringLiteral("SELECT 1 FROM categories WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return false;
    }
    return query.next();
}

bool CategoryRepository::nameExists(const QString &name, const QString &excludeId,
                                    QString *errorMessage) const
{
    QString sql = QStringLiteral("SELECT 1 FROM categories WHERE lower(name) = lower(?)");
    if (!excludeId.isEmpty())
        sql += QStringLiteral(" AND id <> ?");

    QSqlQuery query(m_database.database());
    query.prepare(sql);
    query.addBindValue(name.trimmed());
    if (!excludeId.isEmpty())
        query.addBindValue(excludeId);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return true; // be conservative on errors: treat as taken
    }
    return query.next();
}

int CategoryRepository::snippetCount(const QString &id, QString *errorMessage) const
{
    QSqlQuery query(m_database.database());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM snippets WHERE category_id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return 0;
    }
    return query.next() ? query.value(0).toInt() : 0;
}

bool CategoryRepository::create(const QString &name, const QString &colorKey, Category *created,
                                QString *errorMessage)
{
    const QString validated = validateName(name);
    if (validated.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("分类名称不能为空。");
        return false;
    }
    if (nameExists(validated, QString(), errorMessage)) {
        if (errorMessage)
            *errorMessage = QObject::tr("已存在同名分类：%1").arg(validated);
        return false;
    }

    Category category;
    category.name = validated;
    category.colorKey = colorKey.trimmed().isEmpty() ? nextColorKey(listAll()) : colorKey.trimmed();
    category.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    category.createdAt = nowIso();

    QSqlQuery orderQuery(m_database.database());
    if (orderQuery.exec(QStringLiteral("SELECT COALESCE(MAX(sort_order), 0) FROM categories"))
        && orderQuery.next()) {
        category.sortOrder = orderQuery.value(0).toInt() + 10;
    }

    QSqlQuery query(m_database.database());
    query.prepare(QStringLiteral(
        "INSERT INTO categories (id, name, color_key, sort_order, created_at) "
        "VALUES (?, ?, ?, ?, ?)"));
    query.addBindValue(category.id);
    query.addBindValue(category.name);
    query.addBindValue(category.colorKey);
    query.addBindValue(category.sortOrder);
    query.addBindValue(category.createdAt);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QObject::tr("新建分类失败：%1").arg(query.lastError().text());
        return false;
    }

    qInfo().noquote() << "Category created:" << category.name;
    if (created)
        *created = category;
    return true;
}

bool CategoryRepository::rename(const QString &id, const QString &newName, QString *errorMessage)
{
    const auto category = findById(id, errorMessage);
    if (!category) {
        if (errorMessage && errorMessage->isEmpty())
            *errorMessage = QObject::tr("分类不存在或已被删除。");
        return false;
    }

    const QString validated = validateName(newName);
    if (validated.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("分类名称不能为空。");
        return false;
    }
    if (validated == category->name)
        return true;
    if (nameExists(validated, id, errorMessage)) {
        if (errorMessage)
            *errorMessage = QObject::tr("已存在同名分类：%1").arg(validated);
        return false;
    }

    QSqlQuery query(m_database.database());
    query.prepare(QStringLiteral("UPDATE categories SET name = ? WHERE id = ?"));
    query.addBindValue(validated);
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QObject::tr("重命名失败：%1").arg(query.lastError().text());
        return false;
    }

    qInfo().noquote() << "Category renamed:" << category->name << "->" << validated;
    return true;
}

bool CategoryRepository::remove(const QString &id, int *movedSnippets, QString *errorMessage)
{
    const auto category = findById(id, errorMessage);
    if (!category) {
        if (errorMessage && errorMessage->isEmpty())
            *errorMessage = QObject::tr("分类不存在或已被删除。");
        return false;
    }

    QSqlDatabase db = m_database.database();
    if (!db.transaction()) {
        if (errorMessage)
            *errorMessage = db.lastError().text();
        return false;
    }

    const int affected = snippetCount(id, nullptr);

    // Snippets keep existing without a category (shown as "未分类").
    QSqlQuery clearQuery(db);
    clearQuery.prepare(QStringLiteral(
        "UPDATE snippets SET category_id = NULL, updated_at = ? WHERE category_id = ?"));
    clearQuery.addBindValue(nowIso());
    clearQuery.addBindValue(id);
    if (!clearQuery.exec()) {
        const QString detail = clearQuery.lastError().text();
        db.rollback();
        if (errorMessage)
            *errorMessage = QObject::tr("移动分类内容失败：%1").arg(detail);
        return false;
    }

    QSqlQuery deleteQuery(db);
    deleteQuery.prepare(QStringLiteral("DELETE FROM categories WHERE id = ?"));
    deleteQuery.addBindValue(id);
    if (!deleteQuery.exec()) {
        const QString detail = deleteQuery.lastError().text();
        db.rollback();
        if (errorMessage)
            *errorMessage = QObject::tr("删除分类失败：%1").arg(detail);
        return false;
    }

    if (!db.commit()) {
        const QString detail = db.lastError().text();
        db.rollback();
        if (errorMessage)
            *errorMessage = QObject::tr("删除分类失败：%1").arg(detail);
        return false;
    }

    qInfo().noquote() << "Category deleted:" << category->name << "moved snippets:" << affected;
    if (movedSnippets)
        *movedSnippets = affected;
    return true;
}

QString CategoryRepository::validateName(const QString &name)
{
    return name.trimmed();
}

QString CategoryRepository::nextColorKey(const QList<Category> &existing)
{
    const QStringList palette = paletteKeys();
    return palette.at(existing.size() % palette.size());
}

} // namespace talkbox
