import QtQuick
import QtQuick.Controls
import TalkBox

// Borderless toolbar icon button: quiet by default, soft tint on hover,
// optional toggled (active) state with a blue icon.
Button {
    id: control

    // Declared as url so the caller's literal resolves relative to the
    // caller's file (a plain string would resolve inside this component).
    property url iconSource
    property url activeIconSource
    property bool toggled: false
    property int iconSize: 16

    implicitHeight: Theme.controlHeight
    implicitWidth: Theme.controlHeight
    hoverEnabled: true
    // Tab reaches the button, but a mouse click never gives it focus: a
    // focused toolbar button used to flash a dark focus ring.
    focusPolicy: Qt.TabFocus

    // Same-hue transparent resting colour: avoids a grey hover flash.
    readonly property color hoverTint: Theme.primarySoft
    readonly property color restTint: Qt.rgba(hoverTint.r, hoverTint.g, hoverTint.b, 0)

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.toggled ? Theme.primarySoft
               : control.hovered ? control.hoverTint
               : control.restTint
        Behavior on color {
            ColorAnimation { duration: Theme.animationFast }
        }
    }

    contentItem: Image {
        width: control.iconSize
        height: control.iconSize
        anchors.centerIn: parent
        smooth: true
        // keep every icon in its native proportions
        fillMode: Image.PreserveAspectFit
        source: control.toggled && control.activeIconSource.toString() !== ""
                ? control.activeIconSource
                : control.iconSource
    }
}
