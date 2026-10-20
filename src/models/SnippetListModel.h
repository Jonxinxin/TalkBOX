#pragma once

#include "data/Snippet.h"

#include <QAbstractListModel>

namespace talkbox {

class SnippetRepository;

/// Exposes the filtered snippet list to QML. Filter state (search text,
/// category, type) lives in the model so combined filtering always stays
/// consistent and the result count matches.
class SnippetListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY filterChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY
                   filterChanged)
    Q_PROPERTY(QString typeFilter READ typeFilter WRITE setTypeFilter NOTIFY filterChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)
    Q_PROPERTY(int uncategorizedCount READ uncategorizedCount NOTIFY countChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TypeRole,
        ContentRole,
        PreviewRole,
        DisplayNameRole,
        CategoryIdRole,
        CategoryNameRole,
        CategoryColorKeyRole,
        RelativePathRole,
        AbsolutePathRole,
        AbsoluteUrlRole,
        MimeTypeRole,
        FileSizeRole,
        CreatedAtRole,
        UpdatedAtRole,
    };
    Q_ENUM(Role)

    explicit SnippetListModel(SnippetRepository &repository, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString searchQuery() const;
    void setSearchQuery(const QString &query);

    QString categoryFilter() const;
    void setCategoryFilter(const QString &categoryId);

    QString typeFilter() const;
    void setTypeFilter(const QString &type);

    int totalCount() const;

    /// Number of stored snippets without a category ("未分类"). Independent of
    /// the active filters; used to decide whether the virtual chip is shown.
    int uncategorizedCount() const;

    QString lastError() const;

    Q_INVOKABLE void refresh();

    /// Id of the snippet at the given row, or an empty string when the
    /// row does not exist (e.g. stale keyboard focus after filtering).
    Q_INVOKABLE QString idAt(int row) const;

    /// Complete field map of the given row, or an empty map. Used by
    /// keyboard activation and card menus.
    Q_INVOKABLE QVariantMap itemAt(int row) const;

signals:
    void filterChanged();
    void countChanged();
    void lastErrorChanged();

private:
    void setError(const QString &message);

    SnippetRepository &m_repository;
    QList<Snippet> m_snippets;
    int m_uncategorizedCount = 0;
    QString m_searchQuery;
    QString m_categoryFilter = QStringLiteral("all");
    QString m_typeFilter = QStringLiteral("all");
    QString m_lastError;
};

} // namespace talkbox
