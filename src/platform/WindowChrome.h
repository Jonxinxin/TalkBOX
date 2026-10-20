#pragma once

#include <QAbstractNativeEventFilter>
#include <QColor>
#include <QObject>
#include <QPointer>
#include <QWindow>

class QEvent;

namespace talkbox {

/// Gives the window a QML drawn title bar while keeping the *native* Windows
/// frame underneath.
///
/// The frame bits (WS_CAPTION | WS_THICKFRAME) are never removed, so Windows
/// keeps providing the DWM drop shadow, the resize borders, Aero snap, the
/// system menu and the window animations. Only the caption *painting* is
/// suppressed: WM_NCCALCSIZE is answered with "the client area is the whole
/// window", which lets QML draw the title bar itself.
///
/// Qt rewrites the native window style from QWindow::flags() at creation and
/// whenever the flags or the window state change (this app toggles the
/// stay-on-top flag), and Windows re-themes the frame on DPI / theme changes.
/// Applying the DWM attributes once in the constructor is therefore not
/// enough: WinIdChange / Show / state changes / WM_STYLECHANGED / WM_DPICHANGED
/// re-assert the frame bits and the DWM attributes every time.
class WindowChrome : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit WindowChrome(QObject *parent = nullptr);
    ~WindowChrome() override;

    /// Enables the custom title bar for a top level window. The native window
    /// is created (but not shown) if necessary, so the frame can be prepared
    /// before the first visible frame.
    bool attach(QWindow *window);

    QWindow *window() const { return m_window.data(); }

    /// Colour of the 1px DWM border. Should match Theme.border so activation
    /// never falls back to the system accent / black border.
    void setBorderColor(const QColor &color);

    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           qintptr *result) override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyFrame(bool forceNonClientRecalc);
    void scheduleApply();

    QPointer<QWindow> m_window;
    QColor m_borderColor;
    bool m_filterInstalled = false;
    bool m_applying = false;
    bool m_applyQueued = false;
};

} // namespace talkbox
