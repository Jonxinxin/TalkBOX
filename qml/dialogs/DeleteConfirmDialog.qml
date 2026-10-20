import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TalkBox

// Deletion confirmation. Cancel is the default focus (safe action) and
// the dialog states the exact object and the cleanup scope (PRD 4.5/F-10).
Popup {
    id: dialog
    objectName: "deleteConfirmDialog"

    property string snippetId: ""
    property string snippetType: "text"
    property string describe: ""

    signal confirmed()

    // Adaptive width: the app window can be a narrow side panel.
    width: Math.min(440, (Overlay.overlay ? Overlay.overlay.width : 480) - 28)
    modal: true
    focus: true
    // Esc or a click anywhere outside the panel dismisses it (= 取消).
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay
    padding: 22

    Overlay.modal: DialogScrim {}

    background: Rectangle {
        radius: 14
        color: Theme.cardBackground
        border.width: 1
        border.color: "#E3EAF4"
    }

    function openFor(id, type, describeText) {
        snippetId = id
        snippetType = type
        describe = describeText
        open()
    }

    function titleText() {
        if (snippetType === "image")
            return qsTr("确定删除这张图片吗？")
        if (snippetType === "pdf")
            return qsTr("确定删除这个 PDF 吗？")
        return qsTr("确定删除这条话术吗？")
    }

    onOpened: cancelButton.forceActiveFocus()

    contentItem: ColumnLayout {
        spacing: 12

        Keys.onEscapePressed: dialog.close()

        Text {
            Layout.fillWidth: true
            text: dialog.titleText()
            color: Theme.textPrimary
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }
        Text {
            Layout.fillWidth: true
            text: dialog.describe
            color: Theme.textSecondary
            font.pixelSize: 13
            elide: Text.ElideRight
            maximumLineCount: 2
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }

        Text {
            Layout.fillWidth: true
            visible: dialog.snippetType !== "text"
            text: qsTr("托管附件文件会一并清理，删除后无法恢复。")
            color: Theme.textMuted
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                id: cancelButton
                objectName: "deleteCancelButton"
                text: qsTr("取消")
                variant: "secondary"
                onClicked: dialog.close()
            }

            AppButton {
                objectName: "deleteConfirmButton"
                text: qsTr("删除")
                variant: "danger"
                onClicked: {
                    dialog.close()
                    dialog.confirmed()
                }
            }
        }
    }
}
