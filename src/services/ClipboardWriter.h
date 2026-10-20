#pragma once

#include <QGuiApplication>
#include <QClipboard>
#include <QString>
#include <QThread>

namespace talkbox {

/// Thin seam over the system clipboard so that copy logic is testable
/// without a GUI session.
class ClipboardWriter
{
public:
    virtual ~ClipboardWriter() = default;

    /// Writes text to the clipboard. Returns false when no clipboard is
    /// available.
    virtual bool setText(const QString &text) = 0;

    /// Reads back the current clipboard text (empty when unavailable).
    virtual QString text() const = 0;
};

/// Production implementation backed by QGuiApplication::clipboard().
class SystemClipboardWriter final : public ClipboardWriter
{
public:
    bool setText(const QString &text) override
    {
        QClipboard *clipboard = QGuiApplication::clipboard();
        if (!clipboard)
            return false;
        // Windows can transiently refuse clipboard writes while another
        // process (or a clipboard manager) holds the clipboard. Retry
        // briefly and verify the result instead of pretending success.
        for (int attempt = 0; attempt < 6; ++attempt) {
            clipboard->setText(text, QClipboard::Clipboard);
            if (clipboard->text(QClipboard::Clipboard) == text)
                return true;
            QThread::msleep(30);
        }
        return false;
    }

    QString text() const override
    {
        const QClipboard *clipboard = QGuiApplication::clipboard();
        return clipboard ? clipboard->text(QClipboard::Clipboard) : QString();
    }
};

} // namespace talkbox
