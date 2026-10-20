import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TalkBox

// Rename dialog for image/PDF snippets: edits the display name only; the
// managed file keeps its UUID path (PRD 4.6 / AT-10). The original
// extension is re-appended by the service when it is missing.
Popup {
    id: dialog
    objectName: "renameDialog"

    property string snippetId: ""
    property string snippetType: "image"
    property string originalName: ""
    property bool confirmingDiscard: false

    signal renamed()

    // Adaptive width: the app window can be a narrow side panel.
    width: Math.min(440, (Overlay.overlay ? Overlay.overlay.width : 480) - 28)
    modal: true
    focus: true
    // Esc or a click anywhere outside closes the dialog; a changed name
    // turns that into the discard confirmation (see onClosed).
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

    function openFor(id, displayName, type) {
        snippetId = id
        snippetType = type
        originalName = displayName
        nameField.text = displayName
        confirmingDiscard = false
        errorLabel.visible = false
        open()
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    function requestClose() {
        if (nameField.text.trim() !== originalName.trim()) {
            confirmingDiscard = true
            return
        }
        close()
    }

    function trySave() {
        if (SnippetService.renameAttachment(dialog.snippetId, nameField.text)) {
            close()
            renamed()
        } else {
            errorLabel.text = SnippetService.lastError
            errorLabel.visible = true
        }
    }

    onClosed: {
        // closePolicy (Esc / click outside) reaches here with a pending
        // rename: keep the dialog up and ask before discarding it.
        if (confirmingDiscard !== true
                && nameField.text.trim() !== originalName.trim()) {
            confirmingDiscard = true
            open()
            return
        }
        confirmingDiscard = false
        errorLabel.visible = false
    }

    contentItem: ColumnLayout {
        spacing: 14

        Keys.onEscapePressed: dialog.requestClose()

        Text {
            text: qsTr("重命名")
            color: Theme.textPrimary
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: !dialog.confirmingDiscard

            Text {
                Layout.fillWidth: true
                text: qsTr("当前文件名：%1").arg(dialog.originalName)
                color: Theme.textSecondary
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            TextField {
                id: nameField
                Layout.fillWidth: true
                implicitHeight: 38
                placeholderText: qsTr("输入新的显示文件名")
                placeholderTextColor: "#9BA8BA"
                color: Theme.textPrimary
                font.pixelSize: 14
                selectByMouse: true
                leftPadding: 12
                rightPadding: 12
                verticalAlignment: TextInput.AlignVCenter

                background: Rectangle {
                    radius: Theme.radiusMedium
                    color: "#FBFCFE"
                    border.width: 1
                    border.color: errorLabel.visible ? "#D98A92"
                            : nameField.activeFocus ? Theme.focusBorder
                            : Theme.border
                    Behavior on border.color {
                        ColorAnimation { duration: Theme.animationFast }
                    }
                }

                Keys.onReturnPressed: dialog.trySave()
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("只修改列表中的显示名称，实际文件路径与扩展名不受影响。")
                color: Theme.textMuted
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            Text {
                id: errorLabel
                Layout.fillWidth: true
                visible: false
                color: Theme.danger
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: dialog.confirmingDiscard

            Text {
                Layout.fillWidth: true
                text: qsTr("放弃对文件名的修改？")
                color: Theme.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("尚未保存的修改不会保留。")
                color: Theme.textSecondary
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                text: dialog.confirmingDiscard ? qsTr("继续编辑") : qsTr("取消")
                variant: "secondary"
                onClicked: {
                    if (dialog.confirmingDiscard) {
                        dialog.confirmingDiscard = false
                        nameField.forceActiveFocus()
                    } else {
                        dialog.requestClose()
                    }
                }
            }

            AppButton {
                visible: !dialog.confirmingDiscard
                text: qsTr("保存")
                variant: "primary"
                onClicked: dialog.trySave()
            }

            AppButton {
                visible: dialog.confirmingDiscard
                text: qsTr("放弃")
                variant: "danger"
                onClicked: dialog.close()
            }
        }
    }
}
