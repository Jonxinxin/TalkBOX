#pragma once

#include <QObject>
#include <QString>

namespace talkbox {

class AttachmentService;
class ClipboardWriter;
class SnippetRepository;

/// Business operations on snippets that QML triggers directly: creating
/// text and attachment snippets, editing, renaming, changing category,
/// deleting and copying to the clipboard.
/// Errors are exposed through lastError(); they never fail silently.
class SnippetService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    SnippetService(SnippetRepository &repository, ClipboardWriter &clipboard,
                   AttachmentService &attachments, QObject *parent = nullptr);

    QString lastError() const;

    /// Validates and inserts a text snippet.
    Q_INVOKABLE bool createTextSnippet(const QString &content, const QString &categoryId);

    /// Inserts a snippet for a file that was already imported by
    /// AttachmentService (relativePath points inside attachments/).
    Q_INVOKABLE bool createAttachmentSnippet(const QString &type, const QString &displayName,
                                             const QString &relativePath, const QString &mimeType,
                                             qint64 fileSize, const QString &categoryId);

    /// Updates the body and category of a text snippet.
    Q_INVOKABLE bool updateTextSnippet(const QString &id, const QString &content,
                                       const QString &categoryId);

    /// Updates the display name of an image/PDF snippet. The real file
    /// keeps its UUID path; the original extension is re-appended when
    /// the user removed it.
    Q_INVOKABLE bool renameAttachment(const QString &id, const QString &newDisplayName);

    /// Moves a snippet to another category.
    Q_INVOKABLE bool changeCategory(const QString &id, const QString &categoryId);

    /// Deletes the snippet; the managed file is removed as well when no
    /// other record references it. Cleanup problems are reported through
    /// warningOccurred() but the record is still gone.
    Q_INVOKABLE bool deleteSnippet(const QString &id);

    /// Copies the full text of the snippet with the given id to the
    /// system clipboard. Records last_used_at on success.
    Q_INVOKABLE bool copySnippet(const QString &id);

signals:
    void snippetsChanged();
    void snippetCreated(const QString &id);
    void snippetCopied(const QString &id);
    void snippetDeleted(const QString &id);
    void warningOccurred(const QString &message);
    void lastErrorChanged();

private:
    bool fail(const QString &message);
    void clearError();
    static QString normalizedCategoryId(const QString &categoryId);
    static QString ensureOriginalExtension(const QString &name, const QString &relativePath);

    SnippetRepository &m_repository;
    ClipboardWriter &m_clipboard;
    AttachmentService &m_attachments;
    QString m_lastError;
};

} // namespace talkbox
