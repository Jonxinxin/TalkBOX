#pragma once

#include "data/Category.h"

#include <QAbstractListModel>

namespace talkbox {

class CategoryRepository;

/// Exposes the real (all user created) categories to QML. The virtual "全部"
/// and "未分类" entries are added by the UI layer and never stored.
class CategoryListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ColorKeyRole,
    };
    Q_ENUM(Role)

    explicit CategoryListModel(CategoryRepository &repository, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int totalCount() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool hasCategory(const QString &id) const;

    /// Category for new content: preferredCategoryId when it names an existing
    /// category (and is not a virtual filter), otherwise the first category,
    /// otherwise an empty string ("no category").
    Q_INVOKABLE QString defaultCategoryId(const QString &preferredCategoryId) const;

signals:
    void countChanged();

private:
    CategoryRepository &m_repository;
    QList<Category> m_categories;
};

} // namespace talkbox
