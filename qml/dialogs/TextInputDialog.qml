import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TalkBox

// Small single-field input dialog used for category names. The caller
// validates (via the services) and can push an error back with
// showError(); the dialog only closes when the caller closes it.
Popup {
    id: dialog

    property string title: ""
    property string label: ""
    property string placeholder: ""
    property string errorText: ""

    signal submitted(string text)

    // Adaptive width: the app window can be a narrow side panel.
    width: Math.min(420, (Overlay.overlay ? Overlay.overlay.width : 480) - 28)
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

    function openFor(titleText, labelText, placeholderText, initialText) {
        title = titleText
        label = labelText || ""
        placeholder = placeholderText || ""
        errorText = ""
        input.text = initialText || ""
        open()
        input.forceActiveFocus()
        input.selectAll()
    }

    function showError(message) {
        errorText = message
        input.forceActiveFocus()
    }

    function submit() {
        const value = input.text.trim()
        if (value === "") {
            showError(qsTr("名称不能为空"))
            return
        }
        dialog.submitted(value)
    }

    onClosed: errorText = ""

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
            visible: dialog.label !== ""
            text: dialog.label
            color: Theme.textSecondary
            font.pixelSize: 12
        }

        TextField {
            id: input
            objectName: "textInputField"
            Layout.fillWidth: true
            implicitHeight: 38
            placeholderText: dialog.placeholder
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
                border.color: dialog.errorText !== "" ? "#D98A92"
                        : input.activeFocus ? Theme.focusBorder
                        : Theme.border
                Behavior on border.color {
                    ColorAnimation { duration: Theme.animationFast }
                }
            }

            Keys.onReturnPressed: dialog.submit()
        }

        Text {
            Layout.fillWidth: true
            visible: dialog.errorText !== ""
            text: dialog.errorText
            color: Theme.danger
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                text: qsTr("取消")
                variant: "secondary"
                onClicked: dialog.close()
            }

            AppButton {
                objectName: "textInputSubmitButton"
                text: qsTr("保存")
                variant: "primary"
                onClicked: dialog.submit()
            }
        }
    }
}
