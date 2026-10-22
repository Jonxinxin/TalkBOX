import QtQuick
import TalkBox

// Small iOS-style on/off switch used by the settings pages. `checked` is
// normally bound to a C++ property; the switch only reports the intended
// new state through `toggled()` so it never breaks that binding.
Item {
    id: control

    property bool checked: false
    signal toggled()

    implicitWidth: 42
    implicitHeight: 24
    width: implicitWidth
    height: implicitHeight

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: control.checked ? Theme.primary : "#D6DEE9"

        Behavior on color {
            ColorAnimation { duration: Theme.animationFast }
        }
    }

    Rectangle {
        id: knob
        width: 18
        height: 18
        radius: width / 2
        y: (control.height - height) / 2
        x: control.checked ? control.width - width - 3 : 3
        color: "#FFFFFF"
        border.width: 1
        border.color: control.checked ? "#1D4FA8" : "#C9D3E0"

        Behavior on x {
            NumberAnimation {
                duration: Theme.animationFast
                easing.type: Easing.OutCubic
            }
        }

        Behavior on border.color {
            ColorAnimation { duration: Theme.animationFast }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: control.toggled()
    }
}
