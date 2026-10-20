import QtQuick

// Dimmed backdrop shared by every modal dialog.
//
// The dim is purely decorative: clicks outside a modal popup are swallowed by
// Qt (a modal popup blocks presses to items beneath it, by design), so closing
// is driven by Popup.closePolicy instead. Every dialog therefore uses
// Popup.CloseOnEscape | Popup.CloseOnPressOutside and decides in its
// onClosed handler whether the click really dismisses it or whether the
// dialog must stay open (unsaved input, running import/backup, ...).
Rectangle {
    id: scrim
    objectName: "dialogScrim"

    /// Alpha-tinted dim; the image preview uses a slightly stronger one.
    property color scrimColor: "#591B2A43"

    color: scrimColor
}
