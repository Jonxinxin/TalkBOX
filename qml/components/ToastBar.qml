import QtQuick
import TalkBox

// Short-lived feedback pill (success / error). Hidden by default.
Rectangle {
    id: toast

    property string message: ""
    property bool isError: false

    opacity: 0
    visible: opacity > 0
    color: isError ? "#B23B3B" : "#1F3350"
    radius: Theme.radiusMedium
    implicitWidth: messageText.implicitWidth + 36
    implicitHeight: 40

    function show(text, error) {
        message = text
        isError = !!error
        opacity = 1
        hideTimer.restart()
    }

    Text {
        id: messageText
        anchors.centerIn: parent
        text: toast.message
        color: "white"
        font.pixelSize: 13
    }

    Timer {
        id: hideTimer
        interval: 1800
        onTriggered: toast.opacity = 0
    }

    Behavior on opacity {
        NumberAnimation { duration: 150 }
    }
}
