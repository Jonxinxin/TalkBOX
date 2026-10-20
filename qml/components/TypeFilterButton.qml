import QtQuick
import TalkBox

// Segmented control entry for the type filter (全部内容 / 图片 / PDF).
Rectangle {
    id: button

    property string filterKey: "all"
    property string label: ""
    property string iconName: "grid"

    readonly property bool selected: Snippets.typeFilter === filterKey

    // See CategoryChip: a transparent variant of the hover tint avoids a
    // grey flash while the hover colour animates in.
    readonly property color hoverTint: "#EAF0FA"
    readonly property color restTint: Qt.rgba(hoverTint.r, hoverTint.g, hoverTint.b, 0)

    implicitHeight: 30
    implicitWidth: contentRow.implicitWidth + 22
    radius: 9
    color: selected ? Theme.cardBackground
            : hoverHandler.hovered ? hoverTint
            : restTint
    border.width: selected ? 1 : 0
    border.color: Theme.border

    Behavior on color {
        ColorAnimation { duration: Theme.animationFast }
    }

    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: 6

        Image {
            width: 15
            height: 15
            smooth: true
            fillMode: Image.PreserveAspectFit
            anchors.verticalCenter: parent.verticalCenter
            source: "../icons/" + button.iconName
                    + (button.selected ? "-blue" : "-gray") + ".svg"
        }

        Text {
            text: button.label
            color: button.selected ? Theme.primary : "#6B7F99"
            font.pixelSize: 12
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    HoverHandler {
        id: hoverHandler
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: Snippets.typeFilter = button.filterKey
    }
}
