import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TalkBox

// Generic confirmation dialog (destructive actions default to a safe
// initial focus on "取消", matching the card deletion dialog).
Popup {
    id: dialog

    property string title: ""
    property string message: ""
    property string detail: ""
    property string confirmText: qsTr("确定")
    property bool danger: true

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

    function openFor(titleText, messageText, detailText, confirmLabel, isDanger) {
        title = titleText
        message = messageText
        detail = detailText || ""
        confirmText = confirmLabel || qsTr("确定")
        danger = isDanger === undefined ? true : isDanger
        open()
    }

    onOpened: cancelButton.forceActiveFocus()

    contentItem: ColumnLayout {
        spacing: 12

        Keys.onEscapePressed: dialog.close()

        Text {
            Layout.fillWidth: true
            text: dialog.title
            color: Theme.textPrimary
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            text: dialog.message
            color: Theme.textSecondary
            font.pixelSize: 13
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }

        Text {
            Layout.fillWidth: true
            visible: dialog.detail !== ""
            text: dialog.detail
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
                objectName: "genericCancelButton"
                text: qsTr("取消")
                variant: "secondary"
                onClicked: dialog.close()
            }

            AppButton {
                objectName: "genericConfirmButton"
                text: dialog.confirmText
                variant: dialog.danger ? "danger" : "primary"
                onClicked: {
                    dialog.close()
                    dialog.confirmed()
                }
            }
        }
    }
}
