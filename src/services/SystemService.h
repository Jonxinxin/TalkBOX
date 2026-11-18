#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QTemporaryDir>

namespace talkbox {

/// Thin wrapper around OS integration used by the UI: opening files with
/// their default application, revealing files in Explorer and copying
/// images or files to the clipboard. Every call reports failures through
/// lastError() instead of failing silently.
class SystemService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit SystemService(QObject *parent = nullptr);

    QString lastError() const;

    /// Opens a local file with the system default application (PDF, or
    /// "open original" for images).
    Q_INVOKABLE bool openPath(const QString &absolutePath);

    /// Opens Explorer with the file selected.
    Q_INVOKABLE bool revealInFolder(const QString &absolutePath);

    /// Loads an image file and puts it on the system clipboard.
    Q_INVOKABLE bool copyImageToClipboard(const QString &absolutePath);

    /// Loads an image off the UI thread, then writes it to the clipboard
    /// without blocking QML rendering or animations.
    Q_INVOKABLE void copyImageToClipboardAsync(const QString &absolutePath);

    /// Puts the file itself on the clipboard (a file reference), so it can
    /// be pasted into chat windows or Explorer with Ctrl+V. When displayName
    /// is provided, the clipboard file keeps that user-facing name.
    Q_INVOKABLE bool copyFileToClipboard(const QString &absolutePath,
                                         const QString &displayName = QString());

    /// Creates a temporary file copy whose basename is displayName. The
    /// returned file URL is suitable for drag-and-drop targets such as WeChat.
    Q_INVOKABLE QString prepareFileForTransfer(const QString &absolutePath,
                                               const QString &displayName);

    /// Removes a temporary file created by prepareFileForTransfer().
    Q_INVOKABLE bool releaseFileTransfer(const QString &fileUrl);

    /// Called after a drop was accepted by another application: brings the
    /// window under the cursor to the foreground (when Windows allows it)
    /// and presses Enter once, so chat apps send the dropped message.
    /// Drops that land inside TalkBOX are ignored.
    Q_INVOKABLE void sendEnterAfterDrop();

signals:
    void lastErrorChanged();
    void imageCopyFinished(const QString &absolutePath, bool success,
                           const QString &errorMessage);

private:
    bool fail(const QString &message);
    bool clearError();
    bool writeImageToClipboard(const QImage &image);
    void writeImageToClipboardAsync(const QString &absolutePath, const QImage &image,
                                    int attempt);
    void releaseClipboardTransfer();
    bool isTransferPath(const QString &absolutePath) const;

    QString m_lastError;
    QTemporaryDir m_transferDirectory;
    QString m_clipboardTransferUrl;
};

} // namespace talkbox
