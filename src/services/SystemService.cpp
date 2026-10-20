#include "services/SystemService.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QMimeData>
#include <QProcess>
#include <QThread>
#include <QUrl>

namespace talkbox {

SystemService::SystemService(QObject *parent)
    : QObject(parent)
{
}

QString SystemService::lastError() const
{
    return m_lastError;
}

bool SystemService::openPath(const QString &absolutePath)
{
    if (!QFileInfo::exists(absolutePath))
        return fail(tr("文件不存在或已被移动。"));

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(absolutePath)))
        return fail(tr("无法打开文件：系统中可能没有关联的应用程序。"));
    return clearError();
}

bool SystemService::revealInFolder(const QString &absolutePath)
{
    if (!QFileInfo::exists(absolutePath))
        return fail(tr("文件不存在或已被移动。"));

    const QStringList arguments{QStringLiteral("/select,%1").arg(QDir::toNativeSeparators(absolutePath))};
    if (!QProcess::startDetached(QStringLiteral("explorer.exe"), arguments))
        return fail(tr("无法在文件资源管理器中显示该文件。"));
    return clearError();
}

bool SystemService::copyImageToClipboard(const QString &absolutePath)
{
    if (!QFileInfo::exists(absolutePath))
        return fail(tr("图片文件不存在或已被移动。"));

    QImageReader reader(absolutePath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        return fail(tr("无法读取图片（%1）。").arg(reader.errorString()));
    }

    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return fail(tr("系统剪贴板不可用。"));
    // Same transient-lock retry as for text (clipboard managers etc.).
    for (int attempt = 0; attempt < 6; ++attempt) {
        clipboard->setImage(image, QClipboard::Clipboard);
        if (!clipboard->image(QClipboard::Clipboard).isNull())
            return clearError();
        QThread::msleep(30);
    }
    return fail(tr("无法写入系统剪贴板，请稍后重试。"));
}

bool SystemService::copyFileToClipboard(const QString &absolutePath)
{
    if (!QFileInfo::exists(absolutePath))
        return fail(tr("文件不存在或已被移动。"));

    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return fail(tr("系统剪贴板不可用。"));

    // Same transient-lock retry as for text and images. The clipboard takes
    // ownership of the mime data, so every attempt builds a fresh one.
    for (int attempt = 0; attempt < 8; ++attempt) {
        auto *mime = new QMimeData;
        mime->setUrls({QUrl::fromLocalFile(absolutePath)});
        clipboard->setMimeData(mime, QClipboard::Clipboard);
        const QMimeData *written = clipboard->mimeData(QClipboard::Clipboard);
        if (written && written->hasUrls())
            return clearError();
        QThread::msleep(50);
    }
    return fail(tr("无法写入系统剪贴板，请稍后重试。"));
}

bool SystemService::fail(const QString &message)
{
    if (m_lastError != message) {
        m_lastError = message;
        emit lastErrorChanged();
    }
    return false;
}

bool SystemService::clearError()
{
    if (m_lastError.isEmpty())
        return true;
    m_lastError.clear();
    emit lastErrorChanged();
    return true;
}

} // namespace talkbox
