#include "services/SystemService.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QMimeData>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include <QtConcurrent/QtConcurrentRun>

#include <cstring>
#include <limits>

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

/// Writes a 32-bit top-down DIB directly through Win32. QClipboard::setImage
/// performs format conversion and clipboard IPC on the GUI thread; doing the
/// native write here lets the image copy finish without pausing QML frames.
bool writeNativeClipboardImage(const QImage &source, QString *error)
{
    QImage image = source.convertToFormat(QImage::Format_RGB32);
    const qsizetype pixelBytes = image.sizeInBytes();
    if (image.isNull() || pixelBytes <= 0
        || static_cast<quint64>(pixelBytes) > std::numeric_limits<DWORD>::max()) {
        if (error)
            *error = QObject::tr("图片数据过大，无法写入系统剪贴板。");
        return false;
    }

    const SIZE_T totalBytes = sizeof(BITMAPINFOHEADER) + static_cast<SIZE_T>(pixelBytes);
    HGLOBAL global = ::GlobalAlloc(GMEM_MOVEABLE, totalBytes);
    if (!global) {
        if (error)
            *error = QObject::tr("无法为图片剪贴板数据分配内存。");
        return false;
    }

    auto *header = static_cast<BITMAPINFOHEADER *>(::GlobalLock(global));
    if (!header) {
        ::GlobalFree(global);
        if (error)
            *error = QObject::tr("无法准备图片剪贴板数据。");
        return false;
    }

    *header = {};
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = image.width();
    header->biHeight = -image.height(); // top-down DIB: no row reversal needed
    header->biPlanes = 1;
    header->biBitCount = 32;
    header->biCompression = BI_RGB;
    header->biSizeImage = static_cast<DWORD>(pixelBytes);
    std::memcpy(reinterpret_cast<char *>(header) + sizeof(BITMAPINFOHEADER),
                image.constBits(), static_cast<size_t>(pixelBytes));
    ::GlobalUnlock(global);

    if (!::OpenClipboard(nullptr)) {
        ::GlobalFree(global);
        if (error)
            *error = QObject::tr("系统剪贴板暂时不可用。");
        return false;
    }

    bool success = false;
    if (::EmptyClipboard() && ::SetClipboardData(CF_DIB, global)) {
        success = true;
        global = nullptr; // The clipboard owns it now.
    }
    ::CloseClipboard();

    if (global)
        ::GlobalFree(global);
    if (!success && error)
        *error = QObject::tr("无法写入系统剪贴板，请稍后重试。");
    return success;
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

    releaseClipboardTransfer();

    QImageReader reader(absolutePath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        return fail(tr("无法读取图片（%1）。").arg(reader.errorString()));
    }

    return writeImageToClipboard(image);
}

void SystemService::copyImageToClipboardAsync(const QString &absolutePath)
{
    const QString path = absolutePath;
    if (!QFileInfo::exists(path)) {
        const QString error = tr("图片文件不存在或已被移动。");
        fail(error);
        emit imageCopyFinished(path, false, error);
        return;
    }

#if defined(Q_OS_WIN)
    // Keep both image decoding and the native clipboard transaction off the
    // GUI thread. QClipboard::setImage() can block while Windows accepts and
    // converts a large bitmap, even when the image was decoded in a worker.
    auto *watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this,
            [this, watcher, path]() {
                const QPair<bool, QString> result = watcher->result();
                watcher->deleteLater();
                if (result.first) {
                    clearError();
                    emit imageCopyFinished(path, true, QString());
                } else {
                    fail(result.second);
                    emit imageCopyFinished(path, false, result.second);
                }
            });
    watcher->setFuture(QtConcurrent::run([path]() {
        QPair<bool, QString> result{false, QString()};
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (image.isNull()) {
            result.second = QObject::tr("无法读取图片（%1）。").arg(reader.errorString());
            return result;
        }

        constexpr int maxAttempts = 6;
        for (int attempt = 0; attempt < maxAttempts; ++attempt) {
            QString error;
            if (writeNativeClipboardImage(image, &error))
                return qMakePair(true, QString());
            result.second = error;
            if (attempt + 1 < maxAttempts)
                ::Sleep(30);
        }
        return result;
    }));
#else
    auto *watcher = new QFutureWatcher<QPair<QImage, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<QImage, QString>>::finished, this,
            [this, watcher, path]() {
                const QPair<QImage, QString> result = watcher->result();
                watcher->deleteLater();
                if (!result.second.isEmpty()) {
                    fail(result.second);
                    emit imageCopyFinished(path, false, result.second);
                    return;
                }
                writeImageToClipboardAsync(path, result.first, 0);
            });
    watcher->setFuture(QtConcurrent::run([path]() {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (image.isNull())
            return qMakePair(QImage(), QObject::tr("无法读取图片（%1）。").arg(reader.errorString()));
        return qMakePair(image, QString());
    }));
#endif
}

bool SystemService::writeImageToClipboard(const QImage &image)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return fail(tr("系统剪贴板不可用。"));

    releaseClipboardTransfer();
    // The QML path uses copyImageToClipboardAsync() so these retries never
    // block the UI thread. This synchronous helper remains for tests/callers.
    for (int attempt = 0; attempt < 6; ++attempt) {
        clipboard->setImage(image, QClipboard::Clipboard);
        const QMimeData *written = clipboard->mimeData(QClipboard::Clipboard);
        if (written && written->hasImage())
            return clearError();
        QThread::msleep(30);
    }
    return fail(tr("无法写入系统剪贴板，请稍后重试。"));
}

void SystemService::writeImageToClipboardAsync(const QString &absolutePath,
                                               const QImage &image, int attempt)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        const QString error = tr("系统剪贴板不可用。");
        fail(error);
        emit imageCopyFinished(absolutePath, false, error);
        return;
    }

    if (attempt == 0)
        releaseClipboardTransfer();

    clipboard->setImage(image, QClipboard::Clipboard);
    const QMimeData *written = clipboard->mimeData(QClipboard::Clipboard);
    if (written && written->hasImage()) {
        clearError();
        emit imageCopyFinished(absolutePath, true, QString());
        return;
    }

    constexpr int maxAttempts = 6;
    if (attempt + 1 < maxAttempts) {
        QTimer::singleShot(30, this, [this, absolutePath, image, attempt]() {
            writeImageToClipboardAsync(absolutePath, image, attempt + 1);
        });
        return;
    }

    const QString error = tr("无法写入系统剪贴板，请稍后重试。");
    fail(error);
    emit imageCopyFinished(absolutePath, false, error);
}

bool SystemService::copyFileToClipboard(const QString &absolutePath,
                                        const QString &displayName)
{
    if (!QFileInfo::exists(absolutePath))
        return fail(tr("文件不存在或已被移动。"));

    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return fail(tr("系统剪贴板不可用。"));

    releaseClipboardTransfer();

    QString clipboardPath = absolutePath;
    QString transferUrl;
    if (!displayName.trimmed().isEmpty()) {
        transferUrl = prepareFileForTransfer(absolutePath, displayName);
        if (transferUrl.isEmpty())
            return false;
        clipboardPath = QUrl(transferUrl).toLocalFile();
    }

    // Same transient-lock retry as for text and images. The clipboard takes
    // ownership of the mime data, so every attempt builds a fresh one.
    for (int attempt = 0; attempt < 8; ++attempt) {
        auto *mime = new QMimeData;
        mime->setUrls({QUrl::fromLocalFile(clipboardPath)});
        clipboard->setMimeData(mime, QClipboard::Clipboard);
        const QMimeData *written = clipboard->mimeData(QClipboard::Clipboard);
        if (written && written->hasUrls()) {
            m_clipboardTransferUrl = transferUrl;
            return clearError();
        }
        QThread::msleep(50);
    }
    if (!transferUrl.isEmpty())
        releaseFileTransfer(transferUrl);
    return fail(tr("无法写入系统剪贴板，请稍后重试。"));
}

QString SystemService::prepareFileForTransfer(const QString &absolutePath,
                                              const QString &displayName)
{
    if (!QFileInfo::exists(absolutePath)) {
        fail(tr("文件不存在或已被移动。"));
        return {};
    }

    const QString name = displayName.trimmed();
    const QFileInfo nameInfo(name);
    if (name.isEmpty() || nameInfo.fileName() != name || name == QLatin1String(".")
        || name == QLatin1String("..")) {
        fail(tr("文件名无效。"));
        return {};
    }

    if (!m_transferDirectory.isValid()) {
        fail(tr("无法创建文件传输目录。"));
        return {};
    }

    const QString folder = m_transferDirectory.path() + QLatin1Char('/')
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().mkpath(folder)) {
        fail(tr("无法创建文件传输目录。"));
        return {};
    }

    const QString targetPath = folder + QLatin1Char('/') + name;
    if (!QFile::copy(absolutePath, targetPath)) {
        QDir(folder).removeRecursively();
        fail(tr("无法准备文件传输副本：%1").arg(QFileInfo(targetPath).fileName()));
        return {};
    }

    clearError();
    return QUrl::fromLocalFile(targetPath).toString();
}

bool SystemService::releaseFileTransfer(const QString &fileUrl)
{
    const QUrl url(fileUrl);
    const QString path = url.isLocalFile() ? url.toLocalFile() : fileUrl;
    const QString absolute = QFileInfo(path).absoluteFilePath();
    if (!isTransferPath(absolute))
        return fail(tr("非法的文件传输路径。"));

    if (QFileInfo::exists(absolute) && !QFile::remove(absolute))
        return fail(tr("无法清理文件传输副本。"));
    QDir(QFileInfo(absolute).absolutePath()).removeRecursively();
    if (m_clipboardTransferUrl == fileUrl)
        m_clipboardTransferUrl.clear();
    return clearError();
}

void SystemService::releaseClipboardTransfer()
{
    if (m_clipboardTransferUrl.isEmpty())
        return;

    const QUrl url(m_clipboardTransferUrl);
    const QString path = url.isLocalFile() ? url.toLocalFile() : QString();
    if (!path.isEmpty() && isTransferPath(path)) {
        QFile::remove(path);
        QDir(QFileInfo(path).absolutePath()).removeRecursively();
    }
    m_clipboardTransferUrl.clear();
}

bool SystemService::isTransferPath(const QString &absolutePath) const
{
    const QString root = QDir::toNativeSeparators(
        QFileInfo(m_transferDirectory.path()).absoluteFilePath());
    const QString path = QDir::toNativeSeparators(QFileInfo(absolutePath).absoluteFilePath());
    return m_transferDirectory.isValid()
        && (path == root || path.startsWith(root + QDir::separator(), Qt::CaseInsensitive));
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
