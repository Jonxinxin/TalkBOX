import QtQuick
import TalkBox

// A category filter chip, styled exactly like the type filter buttons
// (TypeFilterButton) so both filter rows look and animate the same.
// The virtual "全部" entry uses categoryKey "all" and hides the colored dot.
Rectangle {
    id: chip

    property string chipText: ""
    property string categoryKey: "all"
    property bool selected: false

    signal clicked()

    // A transparent variant of the hover tint avoids the grey flash that
    // animating from plain "transparent" (transparent black) would cause.
    readonly property color hoverTint: "#EAF0FA"
    readonly property color restTint: Qt.rgba(hoverTint.r, hoverTint.g, hoverTint.b, 0)

    implicitHeight: 30
    implicitWidth: chipRow.implicitWidth + 22
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
        id: chipRow
        anchors.centerIn: parent
        spacing: 6

        Rectangle {
            visible: chip.categoryKey !== "all"
            width: 6
            height: 6
            radius: 3
            color: Theme.chipDot(chip.categoryKey)
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: chip.chipText
            color: chip.selected ? Theme.primary : "#6B7F99"
            font.pixelSize: 12
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    HoverHandler {
        id: hoverHandler
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: chip.clicked()
    }
}
