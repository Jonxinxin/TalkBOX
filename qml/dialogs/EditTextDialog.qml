import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TalkBox

// Edit dialog for text snippets: body + category, with unsaved-changes
// confirmation (PRD 4.6 / F-08).
Popup {
    id: dialog
    objectName: "editTextDialog"

    property string snippetId: ""
    property bool confirmingDiscard: false

    signal saved()

    // Adaptive width: the app window can be a narrow side panel.
    width: Math.min(460, (Overlay.overlay ? Overlay.overlay.width : 480) - 28)
    modal: true
    focus: true
    // Esc or a click anywhere outside the panel closes the dialog. When the
    // text was changed, onClosed turns that into the discard confirmation
    // (by re-opening) so an edit is never lost silently.
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

    property string originalContent: ""

    function openFor(id, content, categoryId) {
        snippetId = id
        originalContent = content
        contentArea.text = content
        selectedCategoryId = categoryId
        confirmingDiscard = false
        validationLabel.visible = false
        open()
        contentArea.forceActiveFocus()
    }

    property string selectedCategoryId: ""

    function isDirty() {
        return contentArea.text.trim() !== originalContent.trim()
    }

    /// Used by the 取消 button and Esc: asks before discarding an edit.
    function requestClose() {
        if (isDirty())
            confirmingDiscard = true
        else
            close()
    }

    function trySave() {
        if (contentArea.text.trim() === "") {
            validationLabel.text = qsTr("请输入话术内容")
            validationLabel.visible = true
            contentArea.forceActiveFocus()
            return
        }
        if (SnippetService.updateTextSnippet(dialog.snippetId, contentArea.text,
                                             dialog.selectedCategoryId)) {
            close()
            saved()
        } else {
            validationLabel.text = SnippetService.lastError
            validationLabel.visible = true
        }
    }

    onClosed: {
        // closePolicy (Esc / click outside) reaches here with pending input:
        // keep the dialog up and ask before discarding.
        if (confirmingDiscard !== true
                && contentArea.text.trim() !== originalContent.trim()) {
            confirmingDiscard = true
            open()
            return
        }
        confirmingDiscard = false
        contentArea.text = ""
        validationLabel.visible = false
    }

    contentItem: ColumnLayout {
        spacing: 14

        Keys.onEscapePressed: dialog.requestClose()

        Text {
            text: qsTr("编辑话术")
            color: Theme.textPrimary
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: !dialog.confirmingDiscard

            TextArea {
                id: contentArea
                objectName: "editTextContentArea"
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                placeholderText: qsTr("输入话术内容…")
                placeholderTextColor: "#9BA8BA"
                wrapMode: TextArea.Wrap
                selectByMouse: true
                color: Theme.textPrimary
                font.pixelSize: 14
                padding: 10

                background: Rectangle {
                    radius: Theme.radiusMedium
                    color: "#FBFCFE"
                    border.width: 1
                    border.color: validationLabel.visible ? "#D98A92"
                            : contentArea.activeFocus ? Theme.focusBorder
                            : Theme.border
                    Behavior on border.color {
                        ColorAnimation { duration: Theme.animationFast }
                    }
                }

                Keys.onPressed: function(event) {
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                            && (event.modifiers & Qt.ControlModifier)) {
                        event.accepted = true
                        dialog.trySave()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: qsTr("分类")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }

                CategoryPicker {
                    Layout.fillWidth: true
                    selectedId: dialog.selectedCategoryId
                    onPicked: function(id) { dialog.selectedCategoryId = id }
                }

                Text {
                    id: validationLabel
                    Layout.fillWidth: true
                    visible: false
                    color: Theme.danger
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: dialog.confirmingDiscard

            Text {
                Layout.fillWidth: true
                text: qsTr("放弃未保存的修改？")
                color: Theme.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("关闭后当前修改不会保留。")
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
                        contentArea.forceActiveFocus()
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
