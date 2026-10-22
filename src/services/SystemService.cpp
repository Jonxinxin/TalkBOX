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
#include <QTimer>
#include <QUrl>

#if defined(Q_OS_WIN)
#  define NOMINMAX
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace talkbox {

namespace {

#if defined(Q_OS_WIN)

HWND topLevelWindowAt(const QPoint &globalPosition)
{
    const POINT point{globalPosition.x(), globalPosition.y()};
    const HWND window = ::WindowFromPoint(point);
    return window ? ::GetAncestor(window, GA_ROOT) : nullptr;
}

DWORD processIdOf(HWND window)
{
    DWORD pid = 0;
    if (window)
        ::GetWindowThreadProcessId(window, &pid);
    return pid;
}

QString windowClassOf(HWND window)
{
    wchar_t buffer[256] = {};
    if (window && ::GetClassNameW(window, buffer, 256) > 0)
        return QString::fromWCharArray(buffer);
    return {};
}

/// Presses and releases Enter in the current foreground window.
bool pressEnter()
{
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_RETURN;
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    return ::SendInput(2, inputs, sizeof(INPUT)) == 2;
}

#endif // Q_OS_WIN

} // namespace

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

void SystemService::sendEnterAfterDrop()
{
#if defined(Q_OS_WIN)
    // GetCursorPos returns native (physical) pixels, which is what
    // WindowFromPoint expects; QCursor::pos() is in device-independent
    // pixels and would pick the wrong window on a scaled display.
    POINT cursor = {};
    if (!::GetCursorPos(&cursor))
        return;
    const HWND target = topLevelWindowAt(QPoint(cursor.x, cursor.y));
    if (!target)
        return;

    // Dropping inside TalkBOX must never press Enter: the focused card would
    // treat it as "copy the current snippet".
    if (processIdOf(target) == ::GetCurrentProcessId()) {
        qInfo("auto-send: drop landed in TalkBOX, Enter not pressed");
        return;
    }

    // Files dropped on the desktop / taskbar / Explorer should stay where they
    // were put: Enter there would open, rename or launch them.
    const QString windowClass = windowClassOf(target);
    if (windowClass == QLatin1String("Progman")
            || windowClass == QLatin1String("WorkerW")
            || windowClass == QLatin1String("Shell_TrayWnd")
            || windowClass == QLatin1String("CabinetWClass")) {
        qInfo("auto-send: drop landed in Explorer, Enter not pressed");
        return;
    }

    // Give the target a moment to finish consuming the drop, then make sure
    // it is the foreground window before typing. Both steps are retried once
    // because some chat clients activate themselves slightly later.
    QTimer::singleShot(150, this, [this, target]() {
        if (!::IsWindow(target))
            return;
        if (::GetForegroundWindow() != target)
            ::SetForegroundWindow(target);

        QTimer::singleShot(250, this, [this, target]() {
            if (!::IsWindow(target))
                return;
            const HWND foreground = ::GetForegroundWindow();
            if (!foreground || ::GetAncestor(foreground, GA_ROOT) != target)
                return;
            if (processIdOf(foreground) == ::GetCurrentProcessId())
                return;

            if (pressEnter())
                qInfo("auto-send: Enter pressed after drag");
            else
                qWarning("auto-send: SendInput failed (target may be elevated)");
        });
    });
#else
    qInfo("auto-send: Enter simulation is only implemented on Windows");
#endif
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
