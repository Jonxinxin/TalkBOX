#pragma once

#include "data/Category.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace talkbox {

class DatabaseManager;

/// Category storage. Every category is user created: the application seeds
/// none, so the list starts empty and the user decides what exists.
class CategoryRepository
{
public:
    explicit CategoryRepository(DatabaseManager &database);

    QList<Category> listAll(QString *errorMessage = nullptr) const;
    std::optional<Category> findById(const QString &id, QString *errorMessage = nullptr) const;
    bool exists(const QString &id, QString *errorMessage = nullptr) const;

    /// Creates a category. An empty colorKey picks the next palette colour;
    /// created (optional) receives the stored row.
    bool create(const QString &name, const QString &colorKey, Category *created = nullptr,
                QString *errorMessage = nullptr);

    bool rename(const QString &id, const QString &newName, QString *errorMessage = nullptr);

    /// Deletes a category; its snippets lose the category (they become
    /// "未分类"). movedSnippets (optional) receives the affected count.
    bool remove(const QString &id, int *movedSnippets = nullptr, QString *errorMessage = nullptr);

    /// Number of snippets assigned to the category.
    int snippetCount(const QString &id, QString *errorMessage = nullptr) const;

    /// True when a different category already uses the name
    /// (case-insensitive, surrounding whitespace ignored).
    bool nameExists(const QString &name, const QString &excludeId = QString(),
                    QString *errorMessage = nullptr) const;

    /// Colour tokens available for categories (low saturation only).
    static QStringList paletteKeys();

private:
    static QString validateName(const QString &name);
    static QString nextColorKey(const QList<Category> &existing);

    DatabaseManager &m_database;
};

} // namespace talkbox
