#pragma once

#include <QObject>
#include <QString>

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

    /// Puts the file itself on the clipboard (a file reference), so it can
    /// be pasted into chat windows or Explorer with Ctrl+V.
    Q_INVOKABLE bool copyFileToClipboard(const QString &absolutePath);

signals:
    void lastErrorChanged();

private:
    bool fail(const QString &message);
    bool clearError();

    QString m_lastError;
};

} // namespace talkbox
