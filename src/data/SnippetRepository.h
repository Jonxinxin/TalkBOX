#pragma once

#include "data/Snippet.h"

#include <QList>
#include <QString>

#include <optional>

class QSqlQuery;

namespace talkbox {

class DatabaseManager;

/// CRUD access to the snippets table. All write operations run inside
/// transactions; listing applies search / category / type filters.
class SnippetRepository
{
public:
    explicit SnippetRepository(DatabaseManager &database);

    std::optional<Snippet> findById(const QString &id, QString *errorMessage = nullptr) const;

    /// Inserts a snippet. Fills id, createdAt and updatedAt on the passed
    /// struct if they are empty.
    bool insert(Snippet &snippet, QString *errorMessage = nullptr);

    /// Updates content, category and attachment display fields.
    /// The caller keeps ownership of timestamp values.
    bool update(const Snippet &snippet, QString *errorMessage = nullptr);

    bool remove(const QString &id, QString *errorMessage = nullptr);

    QList<Snippet> list(const SnippetFilter &filter, QString *errorMessage = nullptr) const;

    /// Number of records referencing the same managed file. Used before
    /// deleting an attachment so shared files are never removed twice.
    int countByRelativePath(const QString &relativePath, QString *errorMessage = nullptr) const;

    /// Records the usage time without touching the sort-relevant
    /// updated_at column. Failures are non-fatal for the caller.
    bool touchLastUsed(const QString &id, QString *errorMessage = nullptr);

    static bool isKnownType(const QString &type);
    static QString newId();
    static QString utcNowIso();

private:
    static Snippet snippetFromQuery(const QSqlQuery &query);
    static QString escapeLike(const QString &term);

    DatabaseManager &m_database;
};

} // namespace talkbox
