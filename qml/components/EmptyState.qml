import QtQuick
import TalkBox

// Centered empty state with a single optional action button.
Item {
    id: emptyState

    property string title: ""
    property string subtitle: ""
    property string actionText: ""
    property bool actionVisible: false

    signal actionClicked()

    Column {
        anchors.centerIn: parent
        width: Math.min(emptyState.width - 80, 360)
        spacing: 10

        Rectangle {
            width: 56
            height: 56
            radius: 16
            color: "#E8EFFB"
            anchors.horizontalCenter: parent.horizontalCenter

            Image {
                anchors.centerIn: parent
                width: 22
                height: 22
                smooth: true
                fillMode: Image.PreserveAspectFit
                source: "../icons/plus-blue.svg"
            }
        }

        Text {
            width: parent.width
            text: emptyState.title
            horizontalAlignment: Text.AlignHCenter
            color: Theme.textPrimary
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        Text {
            width: parent.width
            text: emptyState.subtitle
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.textSecondary
            font.pixelSize: 13
            lineHeight: 1.4
        }

        AppButton {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: emptyState.actionVisible
            text: emptyState.actionText
            variant: "secondary"
            onClicked: emptyState.actionClicked()
        }
    }
}
