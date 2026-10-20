import QtQuick
import QtQuick.Controls
import TalkBox

// Menu entry used by the card context menus. "danger" renders the label
// in the restrained danger color (delete actions).
MenuItem {
    id: control

    property bool danger: false
    /// Unavailable entries are hidden AND collapse to zero height;
    /// invisible items would otherwise still occupy a slot (Qt 6.8).
    property bool available: true

    visible: available
    implicitWidth: 164
    implicitHeight: available ? 34 : 0
    leftPadding: 10
    rightPadding: 10

    contentItem: Text {
        text: control.text
        color: !control.enabled ? Theme.textMuted
               : control.danger ? Theme.danger
               : Theme.textPrimary
        font.pixelSize: 13
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        anchors.margins: 2
        radius: Theme.radiusSmall
        color: control.hovered ? (control.danger ? Theme.dangerSoft : "#F2F6FD") : "transparent"
    }
}
