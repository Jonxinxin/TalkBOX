import QtQuick
import QtQuick.Controls
import QtQuick.Window
import TalkBox

// Custom caption of the window.
//
// The native Windows frame is still there (see src/platform/WindowChrome.cpp):
// DWM owns the drop shadow, the resize borders, Aero snap and the window
// animations, and only the caption *painting* is suppressed. This item paints
// the caption instead, so the window buttons are plain QML controls and can
// never trigger a native non-client repaint (that used to flash dark).
Item {
    id: titleBar

    readonly property var targetWindow: Window.window
    readonly property bool maximized: targetWindow
                                        ? targetWindow.visibility === Window.Maximized
                                        : false
    readonly property int captionHeight: 42

    implicitHeight: captionHeight

    function toggleMaximized() {
        if (!targetWindow)
            return
        if (maximized)
            targetWindow.showNormal()
        else
            targetWindow.showMaximized()
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.titleBarBackground
    }

    // Drag / double-click surface. It stops short of the window buttons so
    // their clicks are never turned into a window move.
    MouseArea {
        id: dragArea
        objectName: "titleBarDragArea"
        anchors.fill: parent
        anchors.rightMargin: captionButtons.width
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        property point pressPos
        property bool moving: false

        onPressed: function(mouse) {
            pressPos = Qt.point(mouse.x, mouse.y)
            moving = false
        }
        onPositionChanged: function(mouse) {
            if (moving || !targetWindow || !(mouse.buttons & Qt.LeftButton))
                return
            if (Math.abs(mouse.x - pressPos.x) < 4 && Math.abs(mouse.y - pressPos.y) < 4)
                return
            moving = true
            // Native window move loop: Aero snap, multi-monitor handling and
            // the restore-on-drag behaviour of maximized windows stay intact.
            targetWindow.startSystemMove()
        }
        onReleased: moving = false
        onCanceled: moving = false
        onDoubleClicked: titleBar.toggleMaximized()
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton)
                captionMenu.popup()
        }
    }

    // Right-click on the caption: the same actions Windows offers in its
    // system menu (minus "move" / "size", which are drag gestures here).
    Menu {
        id: captionMenu
        objectName: "titleBarMenu"

        width: 176
        padding: 6

        background: Rectangle {
            radius: Theme.radiusMedium
            color: Theme.cardBackground
            border.width: 1
            border.color: "#E3EAF4"
        }

        CardMenuItem {
            text: qsTr("还原")
            available: titleBar.maximized
            onTriggered: if (titleBar.targetWindow) titleBar.targetWindow.showNormal()
        }
        CardMenuItem {
            text: qsTr("最小化")
            onTriggered: if (titleBar.targetWindow) titleBar.targetWindow.showMinimized()
        }
        CardMenuItem {
            text: qsTr("最大化")
            available: !titleBar.maximized
            onTriggered: titleBar.toggleMaximized()
        }
        CardMenuItem {
            text: qsTr("关闭")
            danger: true
            onTriggered: if (titleBar.targetWindow) titleBar.targetWindow.close()
        }
    }

    Row {
        id: brand
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Image {
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            source: Qt.resolvedUrl("../icons/app.svg")
            smooth: true
            fillMode: Image.PreserveAspectFit
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("TalkBox")
            color: Theme.textPrimary
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("话术库")
            color: Theme.textMuted
            font.pixelSize: 12
        }
    }

    Row {
        id: captionButtons
        anchors.right: parent.right
        anchors.rightMargin: 0
        anchors.top: parent.top
        spacing: 0

        CaptionButton {
            id: minimizeButton
            objectName: "windowMinimizeButton"
            iconSource: Qt.resolvedUrl("../icons/window-minimize.svg")
            Accessible.name: qsTr("最小化")
            onClicked: if (titleBar.targetWindow) titleBar.targetWindow.showMinimized()
        }

        CaptionButton {
            id: maximizeButton
            objectName: "windowMaximizeButton"
            iconSource: titleBar.maximized
                         ? Qt.resolvedUrl("../icons/window-restore.svg")
                         : Qt.resolvedUrl("../icons/window-maximize.svg")
            Accessible.name: titleBar.maximized ? qsTr("还原") : qsTr("最大化")
            onClicked: titleBar.toggleMaximized()
        }

        CaptionButton {
            id: closeButton
            objectName: "windowCloseButton"
            danger: true
            iconSource: Qt.resolvedUrl("../icons/window-close.svg")
            hoverIconSource: Qt.resolvedUrl("../icons/window-close-white.svg")
            Accessible.name: qsTr("关闭")
            onClicked: if (titleBar.targetWindow) titleBar.targetWindow.close()
        }
    }

    // Hairline between the caption and the content area.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.border
    }
}
