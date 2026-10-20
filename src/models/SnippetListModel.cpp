#include "models/SnippetListModel.h"

#include "app/AppPaths.h"
#include "data/SnippetRepository.h"

#include <QUrl>
#include <QVariantMap>

#include <utility>

namespace talkbox {

namespace {

QString makePreview(const Snippet &snippet)
{
    const QString source = snippet.isText() ? snippet.content : snippet.displayName;
    return source.simplified();
}

QString absolutePathFor(const Snippet &snippet)
{
    if (snippet.relativePath.isEmpty())
        return {};
    return AppPaths::dataDir() + QLatin1Char('/') + snippet.relativePath;
}

QString absoluteUrlFor(const Snippet &snippet)
{
    const QString path = absolutePathFor(snippet);
    return path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
}

} // namespace

SnippetListModel::SnippetListModel(SnippetRepository &repository, QObject *parent)
    : QAbstractListModel(parent)
    , m_repository(repository)
{
    refresh();
}

int SnippetListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_snippets.size());
}

QVariant SnippetListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_snippets.size())
        return {};

    const Snippet &snippet = m_snippets.at(index.row());
    switch (role) {
    case IdRole:
        return snippet.id;
    case TypeRole:
        return snippet.type;
    case ContentRole:
        return snippet.content;
    case PreviewRole:
        return makePreview(snippet);
    case DisplayNameRole:
        return snippet.displayName;
    case CategoryIdRole:
        return snippet.categoryId;
    case CategoryNameRole:
        return snippet.categoryName;
    case CategoryColorKeyRole:
        return snippet.categoryColorKey;
    case RelativePathRole:
        return snippet.relativePath;
    case AbsolutePathRole:
        return absolutePathFor(snippet);
    case AbsoluteUrlRole:
        return absoluteUrlFor(snippet);
    case MimeTypeRole:
        return snippet.mimeType;
    case FileSizeRole:
        return snippet.fileSize;
    case CreatedAtRole:
        return snippet.createdAt;
    case UpdatedAtRole:
        return snippet.updatedAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> SnippetListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TypeRole, "type"},
        {ContentRole, "content"},
        {PreviewRole, "preview"},
        {DisplayNameRole, "displayName"},
        {CategoryIdRole, "categoryId"},
        {CategoryNameRole, "categoryName"},
        {CategoryColorKeyRole, "categoryColorKey"},
        {RelativePathRole, "relativePath"},
        {AbsolutePathRole, "absolutePath"},
        {AbsoluteUrlRole, "absoluteUrl"},
        {MimeTypeRole, "mimeType"},
        {FileSizeRole, "fileSize"},
        {CreatedAtRole, "createdAt"},
        {UpdatedAtRole, "updatedAt"},
    };
}

QString SnippetListModel::searchQuery() const
{
    return m_searchQuery;
}

void SnippetListModel::setSearchQuery(const QString &query)
{
    if (m_searchQuery == query)
        return;
    m_searchQuery = query;
    emit filterChanged();
    refresh();
}

QString SnippetListModel::categoryFilter() const
{
    return m_categoryFilter;
}

void SnippetListModel::setCategoryFilter(const QString &categoryId)
{
    const QString normalized = categoryId.isEmpty() ? QStringLiteral("all") : categoryId;
    if (m_categoryFilter == normalized)
        return;
    m_categoryFilter = normalized;
    emit filterChanged();
    refresh();
}

QString SnippetListModel::typeFilter() const
{
    return m_typeFilter;
}

void SnippetListModel::setTypeFilter(const QString &type)
{
    const QString normalized = type.isEmpty() ? QStringLiteral("all") : type;
    if (m_typeFilter == normalized)
        return;
    m_typeFilter = normalized;
    emit filterChanged();
    refresh();
}

int SnippetListModel::totalCount() const
{
    return static_cast<int>(m_snippets.size());
}

int SnippetListModel::uncategorizedCount() const
{
    return m_uncategorizedCount;
}

QString SnippetListModel::lastError() const
{
    return m_lastError;
}

void SnippetListModel::refresh()
{
    SnippetFilter filter;
    filter.searchQuery = m_searchQuery;
    filter.categoryId = m_categoryFilter;
    filter.type = m_typeFilter;

    QString error;
    QList<Snippet> snippets = m_repository.list(filter, &error);
    if (!error.isEmpty()) {
        // Keep the previously loaded rows visible; the error is surfaced
        // through lastError() instead of silently showing an empty list.
        setError(error);
        return;
    }
    setError(QString());

    // Counted independently of the active filters (the chip that exposes this
    // bucket must stay visible even while another filter is active).
    SnippetFilter uncategorizedFilter;
    uncategorizedFilter.categoryId = SnippetFilter::uncategorizedKey();
    m_uncategorizedCount =
        static_cast<int>(m_repository.list(uncategorizedFilter, nullptr).size());

    beginResetModel();
    m_snippets = std::move(snippets);
    endResetModel();
    emit countChanged();
}

QString SnippetListModel::idAt(int row) const
{
    if (row < 0 || row >= m_snippets.size())
        return {};
    return m_snippets.at(row).id;
}

QVariantMap SnippetListModel::itemAt(int row) const
{
    if (row < 0 || row >= m_snippets.size())
        return {};

    const Snippet &snippet = m_snippets.at(row);
    QVariantMap map;
    map.insert(QStringLiteral("id"), snippet.id);
    map.insert(QStringLiteral("type"), snippet.type);
    map.insert(QStringLiteral("content"), snippet.content);
    map.insert(QStringLiteral("preview"), makePreview(snippet));
    map.insert(QStringLiteral("displayName"), snippet.displayName);
    map.insert(QStringLiteral("categoryId"), snippet.categoryId);
    map.insert(QStringLiteral("categoryName"), snippet.categoryName);
    map.insert(QStringLiteral("categoryColorKey"), snippet.categoryColorKey);
    map.insert(QStringLiteral("relativePath"), snippet.relativePath);
    map.insert(QStringLiteral("absolutePath"), absolutePathFor(snippet));
    map.insert(QStringLiteral("absoluteUrl"), absoluteUrlFor(snippet));
    map.insert(QStringLiteral("mimeType"), snippet.mimeType);
    map.insert(QStringLiteral("fileSize"), snippet.fileSize);
    return map;
}

void SnippetListModel::setError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

} // namespace talkbox
