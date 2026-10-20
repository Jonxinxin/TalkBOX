#pragma once

#include <QObject>
#include <QString>

namespace talkbox {

class CategoryRepository;

/// Category management exposed to QML (PRD F-13): create, rename and delete.
/// Every category is user created; deleting one clears the category of its
/// snippets (they show up as "未分类").
class CategoryService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit CategoryService(CategoryRepository &repository, QObject *parent = nullptr);

    QString lastError() const;

    Q_INVOKABLE bool createCategory(const QString &name);
    Q_INVOKABLE bool renameCategory(const QString &id, const QString &name);
    Q_INVOKABLE bool deleteCategory(const QString &id);

    /// Snippets that would become "未分类" if this category were deleted.
    Q_INVOKABLE int snippetCount(const QString &id) const;

signals:
    void categoriesChanged();
    void lastErrorChanged();

private:
    bool fail(const QString &message);
    void clearError();

    CategoryRepository &m_repository;
    QString m_lastError;
};

} // namespace talkbox
