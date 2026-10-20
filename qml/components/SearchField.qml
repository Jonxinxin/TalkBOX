import QtQuick
import QtQuick.Controls
import TalkBox

// Search input with a leading icon and a clear button. The wrapper owns
// the visual frame; the inner TextField handles the actual editing.
Item {
    id: root

    property alias text: field.text
    property alias placeholderText: field.placeholderText

    implicitHeight: Theme.controlHeight
    implicitWidth: 160

    function activate() {
        field.forceActiveFocus()
        field.selectAll()
    }

    function clear() {
        field.clear()
    }

    TextField {
        id: field
        anchors.fill: parent
        leftPadding: 34
        rightPadding: text.length > 0 ? 30 : 12
        placeholderText: qsTr("搜索话术、图片或 PDF…")
        placeholderTextColor: "#9BA8BA"
        color: Theme.textPrimary
        font.pixelSize: 13
        selectByMouse: true
        verticalAlignment: TextInput.AlignVCenter

        background: Rectangle {
            radius: Theme.radiusMedium
            color: Theme.cardBackground
            border.width: 1
            border.color: field.activeFocus ? Theme.focusBorder : Theme.border
            Behavior on border.color {
                ColorAnimation { duration: Theme.animationFast }
            }
        }
    }

    Image {
        source: "../icons/search.svg"
        width: 15
        height: 15
        smooth: true
        fillMode: Image.PreserveAspectFit
        anchors.left: parent.left
        anchors.leftMargin: 11
        anchors.verticalCenter: parent.verticalCenter
    }

    Image {
        id: clearIcon
        visible: field.text.length > 0
        source: "../icons/close.svg"
        width: 13
        height: 13
        smooth: true
        fillMode: Image.PreserveAspectFit
        opacity: clearArea.containsMouse ? 1.0 : 0.65
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter

        MouseArea {
            id: clearArea
            anchors.fill: parent
            anchors.margins: -6
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clear()
        }
    }
}
