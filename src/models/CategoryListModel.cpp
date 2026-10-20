#include "models/CategoryListModel.h"

#include "data/CategoryRepository.h"
#include "data/Snippet.h"

#include <utility>

namespace talkbox {

CategoryListModel::CategoryListModel(CategoryRepository &repository, QObject *parent)
    : QAbstractListModel(parent)
    , m_repository(repository)
{
    refresh();
}

int CategoryListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_categories.size());
}

QVariant CategoryListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_categories.size())
        return {};

    const Category &category = m_categories.at(index.row());
    switch (role) {
    case IdRole:
        return category.id;
    case NameRole:
        return category.name;
    case ColorKeyRole:
        return category.colorKey;
    default:
        return {};
    }
}

QHash<int, QByteArray> CategoryListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {ColorKeyRole, "colorKey"},
    };
}

int CategoryListModel::totalCount() const
{
    return static_cast<int>(m_categories.size());
}

void CategoryListModel::refresh()
{
    const QList<Category> categories = m_repository.listAll();

    beginResetModel();
    m_categories = categories;
    endResetModel();
    emit countChanged();
}

bool CategoryListModel::hasCategory(const QString &id) const
{
    for (const Category &category : m_categories) {
        if (category.id == id)
            return true;
    }
    return false;
}

QString CategoryListModel::defaultCategoryId(const QString &preferredCategoryId) const
{
    if (preferredCategoryId == SnippetFilter::uncategorizedKey())
        return {}; // the user is looking at uncategorized content
    if (!preferredCategoryId.isEmpty() && preferredCategoryId != QLatin1String("all")
        && hasCategory(preferredCategoryId)) {
        return preferredCategoryId;
    }
    // No built-in categories: fall back to the first user category, or to
    // "no category" when the user has not created any yet.
    return m_categories.isEmpty() ? QString() : m_categories.first().id;
}

} // namespace talkbox
