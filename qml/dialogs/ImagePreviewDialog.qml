import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TalkBox

// In-app image preview (PRD 4.4): large view plus copy / open original /
// reveal in folder. Failures (missing file, clipboard) are shown inline.
//
// The window is a compact side panel, so both dimensions follow the
// available space instead of using fixed values: a fixed 660 x 420 dialog
// used to overflow the panel and clip the action row.
Popup {
    id: dialog
    objectName: "imagePreviewDialog"

    property string snippetId: ""
    property string imageUrl: ""
    property string displayName: ""
    property string absolutePath: ""

    signal copied()

    // The dialog follows the window (the compact panel is narrow, so fixed
    // sizes used to overflow it). Overlay.overlay — not Popup.availableWidth
    // — is the window content here; using the popup's own property would
    // create a binding loop with the popup width.
    readonly property real panelWidth: Overlay.overlay ? Overlay.overlay.width : 480
    readonly property real panelHeight: Overlay.overlay ? Overlay.overlay.height : 720

    width: Math.min(660, Math.max(300, panelWidth - 24))

    /// The preview area takes the remaining height of the window: up to
    /// 420 px on a tall window, smaller on a short one, so the whole dialog
    /// (title + preview + buttons) always fits inside the panel.
    readonly property real previewHeight:
        Math.max(140, Math.min(420, panelHeight - 190))

    modal: true
    focus: true
    // Esc or a click anywhere outside the panel dismisses the preview.
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay
    padding: 18

    Overlay.modal: DialogScrim {
        scrimColor: "#731B2A43"
    }

    background: Rectangle {
        radius: 14
        color: Theme.cardBackground
        border.width: 1
        border.color: "#E3EAF4"
    }

    function openFor(id, url, name, path) {
        snippetId = id
        imageUrl = url
        displayName = name
        absolutePath = path
        errorLabel.visible = false
        open()
        copyButton.forceActiveFocus()
    }

    onClosed: {
        imageUrl = ""
        errorLabel.visible = false
    }

    contentItem: ColumnLayout {
        spacing: 12

        // ---- title row: file name + close ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: dialog.displayName
                color: Theme.textPrimary
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideMiddle
                textFormat: Text.PlainText
            }

            IconButton {
                objectName: "previewCloseButton"
                Layout.alignment: Qt.AlignTop
                iconSource: Qt.resolvedUrl("../icons/close.svg")
                iconSize: 13
                onClicked: dialog.close()
            }
        }

        Rectangle {
            id: previewArea
            objectName: "previewImageArea"
            Layout.fillWidth: true
            Layout.preferredHeight: dialog.previewHeight
            radius: Theme.radiusMedium
            color: "#F4F7FC"
            clip: true

            Image {
                id: preview
                objectName: "previewImage"
                anchors.fill: parent
                anchors.margins: 8
                source: dialog.imageUrl
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: true
                sourceSize.width: 1400
                sourceSize.height: 1000
            }

            Text {
                anchors.centerIn: parent
                visible: preview.status === Image.Loading
                text: qsTr("正在加载图片…")
                color: Theme.textSecondary
                font.pixelSize: 13
            }

            Column {
                anchors.centerIn: parent
                spacing: 8
                visible: preview.status === Image.Error

                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 28
                    height: 28
                    smooth: true
                    fillMode: Image.PreserveAspectFit
                    source: "../icons/image-gray.svg"
                }

                Text {
                    text: qsTr("图片文件缺失或无法读取")
                    color: Theme.danger
                    font.pixelSize: 13
                }
            }
        }

        Text {
            id: errorLabel
            Layout.fillWidth: true
            visible: false
            color: Theme.danger
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        // Flow instead of a row: on a narrow panel the buttons wrap onto a
        // second line rather than being pushed out of the dialog.
        Flow {
            Layout.fillWidth: true
            spacing: 8

            AppButton {
                id: copyButton
                objectName: "previewCopyButton"
                text: qsTr("复制图片")
                variant: "primary"
                onClicked: {
                    if (SystemService.copyImageToClipboard(dialog.absolutePath)) {
                        errorLabel.visible = false
                        dialog.copied()
                    } else {
                        errorLabel.text = SystemService.lastError
                        errorLabel.visible = true
                    }
                }
            }

            AppButton {
                text: qsTr("打开原图")
                variant: "secondary"
                onClicked: {
                    if (!SystemService.openPath(dialog.absolutePath)) {
                        errorLabel.text = SystemService.lastError
                        errorLabel.visible = true
                    }
                }
            }

            AppButton {
                text: qsTr("在文件夹中显示")
                variant: "secondary"
                onClicked: {
                    if (!SystemService.revealInFolder(dialog.absolutePath)) {
                        errorLabel.text = SystemService.lastError
                        errorLabel.visible = true
                    }
                }
            }
        }
    }
}
