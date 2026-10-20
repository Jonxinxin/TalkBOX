#include "services/SnippetService.h"

#include "data/Snippet.h"
#include "data/SnippetRepository.h"
#include "services/AttachmentService.h"
#include "services/ClipboardWriter.h"

#include <QFileInfo>
#include <QRegularExpression>

#include <optional>

namespace talkbox {

namespace {

const QRegularExpression &invalidNameCharacters()
{
    static const QRegularExpression expression(QStringLiteral("[\\\\/:*?\"<>|\\r\\n]"));
    return expression;
}

} // namespace

SnippetService::SnippetService(SnippetRepository &repository, ClipboardWriter &clipboard,
                               AttachmentService &attachments, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
    , m_clipboard(clipboard)
    , m_attachments(attachments)
{
}

QString SnippetService::lastError() const
{
    return m_lastError;
}

bool SnippetService::createTextSnippet(const QString &content, const QString &categoryId)
{
    const QString trimmed = content.trimmed();
    if (trimmed.isEmpty())
        return fail(tr("请输入话术内容。"));

    Snippet snippet;
    snippet.type = QStringLiteral("text");
    snippet.content = trimmed;
    snippet.categoryId = normalizedCategoryId(categoryId);

    QString error;
    if (!m_repository.insert(snippet, &error))
        return fail(error);

    clearError();
    emit snippetsChanged();
    emit snippetCreated(snippet.id);
    return true;
}

bool SnippetService::createAttachmentSnippet(const QString &type, const QString &displayName,
                                             const QString &relativePath, const QString &mimeType,
                                             qint64 fileSize, const QString &categoryId)
{
    if (type != QLatin1String("image") && type != QLatin1String("pdf"))
        return fail(tr("未知的内容类型。"));
    if (relativePath.trimmed().isEmpty()
        || !relativePath.startsWith(AttachmentService::attachmentsPrefix())) {
        return fail(tr("附件路径无效。"));
    }
    if (!m_attachments.managedFileExists(relativePath))
        return fail(tr("附件副本不存在，请重新选择文件。"));

    QString name = displayName.trimmed();
    if (name.isEmpty())
        name = QFileInfo(relativePath).fileName();
    name = ensureOriginalExtension(name, relativePath);

    Snippet snippet;
    snippet.type = type;
    snippet.displayName = name;
    snippet.relativePath = relativePath;
    snippet.mimeType = mimeType;
    snippet.fileSize = fileSize;
    snippet.categoryId = normalizedCategoryId(categoryId);

    QString error;
    if (!m_repository.insert(snippet, &error))
        return fail(error);

    clearError();
    emit snippetsChanged();
    emit snippetCreated(snippet.id);
    return true;
}

bool SnippetService::updateTextSnippet(const QString &id, const QString &content,
                                       const QString &categoryId)
{
    const QString trimmed = content.trimmed();
    if (trimmed.isEmpty())
        return fail(tr("请输入话术内容。"));

    QString error;
    auto snippet = m_repository.findById(id.trimmed(), &error);
    if (!snippet)
        return fail(error.isEmpty() ? tr("话术不存在或已被删除。") : error);
    if (!snippet->isText())
        return fail(tr("只有文本话术支持编辑内容。"));

    snippet->content = trimmed;
    snippet->categoryId = normalizedCategoryId(categoryId);

    if (!m_repository.update(*snippet, &error))
        return fail(error);

    clearError();
    emit snippetsChanged();
    return true;
}

bool SnippetService::renameAttachment(const QString &id, const QString &newDisplayName)
{
    const QString name = newDisplayName.trimmed();
    if (name.isEmpty())
        return fail(tr("请输入文件名。"));
    if (name.contains(invalidNameCharacters()))
        return fail(tr("文件名不能包含 \\ / : * ? \" < > | 等字符。"));

    QString error;
    auto snippet = m_repository.findById(id.trimmed(), &error);
    if (!snippet)
        return fail(error.isEmpty() ? tr("内容不存在或已被删除。") : error);
    if (snippet->isText())
        return fail(tr("文本话术不支持重命名。"));

    const QString finalName = ensureOriginalExtension(name, snippet->relativePath);
    if (finalName == snippet->displayName) {
        clearError();
        return true; // nothing to do
    }

    snippet->displayName = finalName;
    if (!m_repository.update(*snippet, &error))
        return fail(error);

    clearError();
    emit snippetsChanged();
    return true;
}

bool SnippetService::changeCategory(const QString &id, const QString &categoryId)
{
    QString error;
    auto snippet = m_repository.findById(id.trimmed(), &error);
    if (!snippet)
        return fail(error.isEmpty() ? tr("内容不存在或已被删除。") : error);

    // An empty target (or the "未分类" token) removes the category.
    const QString target = normalizedCategoryId(categoryId);
    if (snippet->categoryId == target) {
        clearError();
        return true;
    }

    snippet->categoryId = target;
    if (!m_repository.update(*snippet, &error))
        return fail(error);

    clearError();
    emit snippetsChanged();
    return true;
}

bool SnippetService::deleteSnippet(const QString &id)
{
    QString error;
    const auto snippet = m_repository.findById(id.trimmed(), &error);
    if (!snippet)
        return fail(error.isEmpty() ? tr("内容不存在或已被删除。") : error);

    // Consistency strategy: the database row is the source of truth and
    // is deleted first; the managed file is then cleaned up when no
    // remaining record references it. A failed file removal is reported
    // (warning + log) but must not resurrect the record.
    const bool hasAttachment =
        !snippet->isText() && !snippet->relativePath.trimmed().isEmpty();
    const int references = hasAttachment
        ? m_repository.countByRelativePath(snippet->relativePath, &error)
        : 0;

    if (!m_repository.remove(snippet->id, &error))
        return fail(error);

    if (hasAttachment && references <= 1) {
        if (!m_attachments.removeManagedFile(snippet->relativePath)) {
            qWarning().noquote() << "Attachment cleanup failed for" << snippet->relativePath
                                 << ":" << m_attachments.lastError();
            emit warningOccurred(
                tr("记录已删除，但附件文件清理失败，可稍后在数据目录中手动删除。"));
        }
    }

    clearError();
    emit snippetsChanged();
    emit snippetDeleted(snippet->id);
    return true;
}

bool SnippetService::copySnippet(const QString &id)
{
    QString error;
    const std::optional<Snippet> snippet = m_repository.findById(id.trimmed(), &error);
    if (!snippet)
        return fail(error.isEmpty() ? tr("话术不存在或已被删除。") : error);

    if (!snippet->isText())
        return fail(tr("该内容类型不支持复制文本，请使用预览或打开操作。"));

    if (!m_clipboard.setText(snippet->content))
        return fail(tr("无法写入系统剪贴板，请稍后重试。"));

    // Usage time is bookkeeping only; a failure must not affect copying.
    m_repository.touchLastUsed(snippet->id);

    clearError();
    emit snippetCopied(snippet->id);
    return true;
}

bool SnippetService::fail(const QString &message)
{
    m_lastError = message;
    emit lastErrorChanged();
    return false;
}

void SnippetService::clearError()
{
    if (m_lastError.isEmpty())
        return;
    m_lastError.clear();
    emit lastErrorChanged();
}

QString SnippetService::normalizedCategoryId(const QString &categoryId)
{
    const QString trimmed = categoryId.trimmed();
    // Empty (or the "未分类" filter token) means "no category": there are no
    // built-in categories to fall back to.
    if (trimmed == SnippetFilter::uncategorizedKey())
        return {};
    return trimmed;
}

QString SnippetService::ensureOriginalExtension(const QString &name, const QString &relativePath)
{
    const QString extension = QFileInfo(relativePath).suffix();
    if (extension.isEmpty())
        return name;
    if (name.endsWith(QLatin1Char('.') + extension, Qt::CaseInsensitive))
        return name;
    return name + QLatin1Char('.') + extension;
}

} // namespace talkbox
