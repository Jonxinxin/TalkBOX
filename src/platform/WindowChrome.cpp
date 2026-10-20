#include "platform/WindowChrome.h"

#include <QEvent>
#include <QGuiApplication>
#include <QTimer>
#include <QWindow>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <windowsx.h>
#  include <dwmapi.h>
#endif

namespace talkbox {

#ifdef Q_OS_WIN

namespace {

// DWM attributes that are missing from some SDK headers. The numeric values
// are part of the stable public ABI.
constexpr DWORD kDwmwaUseImmersiveDarkMode = 20;
constexpr DWORD kDwmwaWindowCornerPreference = 33;
constexpr DWORD kDwmwaBorderColor = 34;

constexpr DWORD kDwmwcpRound = 2;

/// Width of the invisible resize band at the window edges, in DIP. The native
/// frame uses the same kind of band, but WM_NCCALCSIZE removed the non-client
/// area, so the hit test has to reproduce it.
constexpr int kResizeBandDip = 4;

LONG_PTR requiredStyleBits()
{
    return WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
}

} // namespace

#endif // Q_OS_WIN

WindowChrome::WindowChrome(QObject *parent)
    : QObject(parent)
    , m_borderColor(0xDD, 0xE5, 0xF0)
{
}

WindowChrome::~WindowChrome()
{
    if (m_filterInstalled && qApp)
        qApp->removeNativeEventFilter(this);
    if (m_window)
        m_window->removeEventFilter(this);
}

void WindowChrome::setBorderColor(const QColor &color)
{
    if (!color.isValid() || color == m_borderColor)
        return;
    m_borderColor = color;
    applyFrame(false);
}

bool WindowChrome::attach(QWindow *window)
{
    if (!window)
        return false;

    if (m_window && m_window != window) {
        m_window->removeEventFilter(this);
        m_window = nullptr;
    }

    m_window = window;

#ifdef Q_OS_WIN
    // winId() creates the native window without showing it.
    if (!window->winId())
        return false;

    window->installEventFilter(this);

    if (!m_filterInstalled && qApp) {
        qApp->installNativeEventFilter(this);
        m_filterInstalled = true;
    }

    // While the real frame still exists, remember where its top-left sits and
    // how large the client (i.e. QML) area is. Those two are what the QML
    // window asked for: the position refers to the frame, the size to the
    // content. After WM_NCCALCSIZE removes the non-client area Qt measures the
    // client rect, so without restoring these the window would grow and shift
    // by the frame thickness.
    RECT outer{};
    RECT client{};
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const bool haveGeometry = hwnd && GetWindowRect(hwnd, &outer)
                              && GetClientRect(hwnd, &client);

    applyFrame(true);

    if (haveGeometry && hwnd) {
        SetWindowPos(hwnd, nullptr, outer.left, outer.top, client.right, client.bottom,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
#else
    window->installEventFilter(this);
    Q_UNUSED(m_filterInstalled)
#endif

    return true;
}

bool WindowChrome::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window) {
        switch (event->type()) {
        case QEvent::WinIdChange:
        case QEvent::Show:
            applyFrame(false);
            // Qt may still be finishing its own style / geometry work when the
            // event is delivered; confirm once the event loop is idle.
            scheduleApply();
            break;
        case QEvent::WindowStateChange:
        case QEvent::DevicePixelRatioChange:
            applyFrame(false);
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

void WindowChrome::scheduleApply()
{
    if (m_applyQueued)
        return;
    m_applyQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_applyQueued = false;
        applyFrame(false);
    });
}

bool WindowChrome::nativeEventFilter(const QByteArray &eventType, void *message,
                                     qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType != QByteArrayLiteral("windows_generic_MSG") || !m_window)
        return false;

    auto *msg = static_cast<MSG *>(message);
    const HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    if (!hwnd || msg->hwnd != hwnd)
        return false;

    switch (msg->message) {
    case WM_NCCALCSIZE:
        if (msg->wParam) {
            // Keep the native frame (shadow, resize edges, snap) but drop the
            // non-client *area*: the client rectangle becomes the whole
            // window, so QML can paint the title bar edge to edge.
            //
            // A native frame is inflated beyond the monitor work area while
            // maximized, which would push the caption off-screen. Clamping the
            // client rect to the work area keeps the QML title bar at the top
            // edge of the screen (the inflated borders hang off-screen, exactly
            // like they do for a normal maximized window).
            if (IsZoomed(hwnd)) {
                MONITORINFO monitorInfo{};
                monitorInfo.cbSize = sizeof(monitorInfo);
                if (GetMonitorInfoW(
                        MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
                        &monitorInfo)) {
                    auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
                    params->rgrc[0] = monitorInfo.rcWork;
                }
            }
            *result = 0;
            return true;
        }
        // Same answer for the single-rect form: the client area is the window
        // (or its work area while maximized).
        {
            RECT target{};
            if (IsZoomed(hwnd)) {
                MONITORINFO monitorInfo{};
                monitorInfo.cbSize = sizeof(monitorInfo);
                if (!GetMonitorInfoW(
                        MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
                        &monitorInfo))
                    break;
                target = monitorInfo.rcWork;
            } else if (!GetWindowRect(hwnd, &target)) {
                break;
            }
            *reinterpret_cast<RECT *>(msg->lParam) = target;
            *result = 0;
            return true;
        }

    case WM_NCHITTEST: {
        const LRESULT hit = [&]() -> LRESULT {
            RECT rc{};
            if (!GetWindowRect(hwnd, &rc))
                return HTNOWHERE;

            const int width = rc.right - rc.left;
            const int height = rc.bottom - rc.top;
            const int x = GET_X_LPARAM(msg->lParam) - rc.left;
            const int y = GET_Y_LPARAM(msg->lParam) - rc.top;
            if (x < 0 || y < 0 || x >= width || y >= height)
                return HTNOWHERE;

            // A maximized window has nothing to resize; the caption is
            // restored by dragging, which Windows handles itself.
            if (IsZoomed(hwnd))
                return HTNOWHERE;

            const int band = qMax(
                1, qRound(kResizeBandDip * m_window->devicePixelRatio()));
            const bool left = x < band;
            const bool right = x >= width - band;
            const bool top = y < band;
            const bool bottom = y >= height - band;

            if (top && left)
                return HTTOPLEFT;
            if (top && right)
                return HTTOPRIGHT;
            if (bottom && left)
                return HTBOTTOMLEFT;
            if (bottom && right)
                return HTBOTTOMRIGHT;
            if (left)
                return HTLEFT;
            if (right)
                return HTRIGHT;
            if (top)
                return HTTOP;
            if (bottom)
                return HTBOTTOM;
            return HTNOWHERE;
        }();

        if (hit != HTNOWHERE) {
            *result = hit;
            return true;
        }
        break;
    }

    case WM_NCACTIVATE:
        // There is no system caption to repaint. Answering directly avoids the
        // one-frame non-client repaint that shows up as a dark shadow / border
        // flash when focus moves between the window and its buttons.
        *result = TRUE;
        return true;

    case WM_STYLECHANGED:
    case WM_THEMECHANGED:
    case WM_DPICHANGED:
        // Qt rewrites the style from QWindow::flags() on flag / state changes
        // and Windows re-themes the frame on DPI / theme changes. Re-assert
        // instead of relying on the setup done at construction time.
        scheduleApply();
        break;

    default:
        break;
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif

    return false;
}

void WindowChrome::applyFrame(bool forceNonClientRecalc)
{
#ifdef Q_OS_WIN
    if (!m_window || m_applying)
        return;

    const HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    if (!hwnd)
        return;

    if (m_window->windowStates() & Qt::WindowFullScreen)
        return;

    m_applying = true;

    const LONG_PTR required = requiredStyleBits();
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    if ((style & required) != required) {
        SetWindowLongPtrW(hwnd, GWL_STYLE, style | required);
        forceNonClientRecalc = true;
    }

    // Light chrome: without this Windows may keep a dark DWM border while the
    // system theme is dark, which reads as a black outline around a light QML
    // window.
    const BOOL dark = FALSE;
    DwmSetWindowAttribute(hwnd, kDwmwaUseImmersiveDarkMode, &dark, sizeof(dark));

    // WM_NCCALCSIZE removed the non-client area, and DWM only draws the drop
    // shadow for windows that still have a frame region. Extending the frame by
    // one invisible pixel (the QML surface covers it) gives DWM that region
    // back, so the shadow is the native one - no QML blur layer, no "fake"
    // shadow, and no black band: the extended pixel is opaque client content.
    const MARGINS frameMargins{1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &frameMargins);

    const DWORD nonClientRendering = DWMNCRP_ENABLED;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &nonClientRendering,
                          sizeof(nonClientRendering));

    // Rounded corners on Windows 11 (the attribute is rejected on Windows 10,
    // where the window keeps its native square frame), and a DWM border in the
    // same colour as the QML separators, so activation never swaps in the
    // system accent colour.
    const DWORD corner = kDwmwcpRound;
    DwmSetWindowAttribute(hwnd, kDwmwaWindowCornerPreference, &corner, sizeof(corner));

    const COLORREF border = RGB(m_borderColor.red(), m_borderColor.green(),
                                m_borderColor.blue());
    DwmSetWindowAttribute(hwnd, kDwmwaBorderColor, &border, sizeof(border));

    if (forceNonClientRecalc) {
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                         | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }

    m_applying = false;
#else
    Q_UNUSED(forceNonClientRecalc)
#endif
}

} // namespace talkbox
