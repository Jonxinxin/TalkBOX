#pragma once

#include <QString>

namespace talkbox {

/// One library entry: a text snippet, an image or a PDF.
/// Mirrors the `snippets` table (see Migrations.cpp).
struct Snippet
{
    QString id;               // UUID
    QString type;             // "text" | "image" | "pdf"
    QString content;          // text body (text entries only)
    QString categoryId;       // empty = no category
    QString categoryName;     // joined from categories (read only)
    QString categoryColorKey; // joined from categories (read only)
    QString displayName;      // attachment display name (image/pdf only)
    QString relativePath;     // attachment path relative to the data dir
    QString mimeType;
    qint64 fileSize = -1;
    QString createdAt;        // ISO 8601 UTC
    QString updatedAt;        // ISO 8601 UTC
    QString lastUsedAt;
    int sortOrder = 0;

    bool isValid() const { return !id.isEmpty(); }
    bool isText() const { return type == QLatin1String("text"); }
};

/// Filter applied when listing snippets. Empty values (or "all") mean
/// "no restriction".
struct SnippetFilter
{
    QString searchQuery; // matched against content, display name and category name
    QString categoryId;  // empty or "all" = every category,
                         // "uncategorized" = only entries without a category
    QString type;        // empty or "all" = every type

    /// Filter token for "no category". Category ids are UUIDs, so this can
    /// never collide with a real category.
    static QString uncategorizedKey() { return QStringLiteral("uncategorized"); }
};

} // namespace talkbox
