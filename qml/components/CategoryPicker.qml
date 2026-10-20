import QtQuick
import TalkBox

// Category chips shared by the create/edit/change-category dialogs.
// Every category is user created, so the picker also offers "未分类" (no
// category). Dialogs show their own hint when the list is still empty.
Flow {
    id: picker

    property string selectedId: ""
    /// Show the virtual "未分类" pill (no category).
    property bool noneVisible: true

    signal picked(string id)

    spacing: 6

    Repeater {
        model: Categories

        Rectangle {
            id: pill

            readonly property bool isSelected: picker.selectedId === model.id

            width: pillText.implicitWidth + 20
            height: 28
            radius: Theme.radiusSmall
            color: isSelected ? Theme.primarySoft : "#F4F7FC"
            border.width: isSelected ? 1.5 : 1
            border.color: isSelected ? Theme.primary : Theme.border

            Behavior on color {
                ColorAnimation { duration: Theme.animationFast }
            }

            Text {
                id: pillText
                anchors.centerIn: parent
                text: model.name
                color: pill.isSelected ? Theme.primary : "#536987"
                font.pixelSize: 12
                font.weight: pill.isSelected ? Font.DemiBold : Font.Normal
            }

            TapHandler {
                onTapped: picker.picked(model.id)
            }
        }
    }

    // No category: stored as an empty category id.
    Rectangle {
        id: nonePill
        objectName: "categoryPickerNone"
        visible: picker.noneVisible
        readonly property bool isSelected: picker.selectedId === ""

        width: noneText.implicitWidth + 20
        height: 28
        radius: Theme.radiusSmall
        color: isSelected ? Theme.primarySoft : "#F4F7FC"
        border.width: isSelected ? 1.5 : 1
        border.color: isSelected ? Theme.primary : Theme.border

        Behavior on color {
            ColorAnimation { duration: Theme.animationFast }
        }

        Text {
            id: noneText
            anchors.centerIn: parent
            text: qsTr("未分类")
            color: nonePill.isSelected ? Theme.primary : "#536987"
            font.pixelSize: 12
            font.weight: nonePill.isSelected ? Font.DemiBold : Font.Normal
        }

        TapHandler {
            onTapped: picker.picked("")
        }
    }
}
