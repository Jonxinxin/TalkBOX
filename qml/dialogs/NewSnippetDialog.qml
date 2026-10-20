import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import TalkBox

// Create dialog (PRD 3.2/3.3, 4.6): step 1 picks the content type,
// step 2 shows the matching form. Attachments are copied into the
// managed directory asynchronously; the dialog stays busy until the
// import finishes and cleans up if the record cannot be created.
Popup {
    id: dialog
    objectName: "newSnippetDialog"

    property string step: "type"        // "type" | "form"
    property string contentType: "text" // "text" | "image" | "pdf"
    property string selectedCategoryId: ""
    property bool confirmingDiscard: false
    property bool importing: false
    property string pendingToken: ""
    property string pickedUrl: ""
    property string pickedError: ""
    /// Only keyboard navigation may draw the tile outline. Opening the dialog
    /// focuses the first tile (so Enter / Space work immediately), but a focus
    /// outline there would look like a selection the user never made.
    property bool keyboardFocusVisible: false

    signal saved()

    // Adaptive width: the app window can be a narrow side panel.
    width: Math.min(460, (Overlay.overlay ? Overlay.overlay.width : 480) - 28)
    modal: true
    focus: true
    // Esc or a click anywhere outside the panel closes the dialog, but
    // unsaved input is never dropped silently: onClosed re-opens it and
    // shows the discard confirmation instead.
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

    FileDialog {
        id: filePicker
        title: dialog.contentType === "pdf" ? qsTr("选择 PDF 文件") : qsTr("选择图片")
        fileMode: FileDialog.OpenFile
        nameFilters: dialog.contentType === "pdf"
            ? [qsTr("PDF 文件 (*.pdf)")]
            : [qsTr("图片 (*.png *.jpg *.jpeg *.webp *.bmp)")]
        onAccepted: dialog.filePicked(selectedFile)
    }

    /// Opens the create dialog. keyboardInitiated is true when the dialog was
    /// opened from a keyboard shortcut: only then the first tile is outlined,
    /// so a mouse click never paints a "selection" the user did not make.
    /// The tile is focused either way, so typing / Enter works immediately.
    function openForCreate(keyboardInitiated) {
        step = "type"
        contentType = "text"
        confirmingDiscard = false
        importing = false
        pendingToken = ""
        pickedUrl = ""
        pickedError = ""
        keyboardFocusVisible = keyboardInitiated === true
        contentArea.text = ""
        displayNameField.text = ""
        validationLabel.visible = false
        selectedCategoryId = Categories.defaultCategoryId(Snippets.categoryFilter)
        open()
        typeTextTile.forceActiveFocus()
    }

    function chooseType(type) {
        if (contentType !== type) {
            pickedUrl = ""
            pickedError = ""
            displayNameField.text = ""
        }
        contentType = type
        step = "form"
        // Focus on the next frame: the target control only becomes
        // visible in this frame, and focusing a hidden item fails.
        if (type === "text")
            Qt.callLater(function() { contentArea.forceActiveFocus() })
        else
            Qt.callLater(function() { pickFileButton.forceActiveFocus() })
    }

    function pickFile() {
        if (dialog.importing)
            return
        pickedError = ""
        filePicker.open()
    }

    function filePicked(url) {
        pickedUrl = url.toString()
        var name = pickedUrl
        var slash = name.lastIndexOf("/")
        if (slash >= 0)
            name = name.substring(slash + 1)
        displayNameField.text = decodeURIComponent(name)
        pickedError = ""
    }

    function pickedSizeText() {
        if (pickedUrl === "")
            return qsTr("尚未选择文件")
        const size = Attachments.fileSizeFor(pickedUrl)
        if (size < 0)
            return qsTr("已选择文件")
        if (size < 1024)
            return size + " B"
        if (size < 1024 * 1024)
            return (size / 1024).toFixed(0) + " KB"
        return (size / (1024 * 1024)).toFixed(1) + " MB"
    }

    function isDirty() {
        if (step !== "form")
            return false
        if (contentType === "text")
            return contentArea.text.trim() !== ""
        return pickedUrl !== ""
    }

    /// Used by the 取消 button: asks before discarding unsaved input.
    function requestClose() {
        if (importing)
            return
        if (isDirty())
            confirmingDiscard = true
        else
            close()
    }

    function backToType() {
        if (importing)
            return
        step = "type"
        validationLabel.visible = false
        pickedError = ""
        typeTextTile.forceActiveFocus()
    }

    function trySave() {
        if (importing)
            return
        if (contentType === "text")
            trySaveText()
        else
            trySaveAttachment()
    }

    function trySaveText() {
        if (contentArea.text.trim() === "") {
            validationLabel.text = qsTr("请输入话术内容")
            validationLabel.visible = true
            contentArea.forceActiveFocus()
            return
        }
        if (SnippetService.createTextSnippet(contentArea.text, dialog.selectedCategoryId)) {
            close()
            saved()
        } else {
            validationLabel.text = SnippetService.lastError
            validationLabel.visible = true
        }
    }

    function trySaveAttachment() {
        if (pickedUrl === "") {
            pickedError = qsTr("请先选择文件")
            return
        }
        if (displayNameField.text.trim() === "") {
            pickedError = qsTr("请输入显示文件名")
            return
        }
        importing = true
        pendingToken = Attachments.startImport(pickedUrl, contentType)
        if (pendingToken === "") {
            importing = false
            pickedError = Attachments.lastError
        }
    }

    Connections {
        target: Attachments

        function onImportFinished(token, ok, error, relativePath, mimeType, fileSize) {
            if (token !== dialog.pendingToken)
                return
            dialog.importing = false
            if (!ok) {
                dialog.pickedError = error
                return
            }
            if (SnippetService.createAttachmentSnippet(dialog.contentType,
                                                       displayNameField.text, relativePath,
                                                       mimeType, fileSize,
                                                       dialog.selectedCategoryId)) {
                dialog.close()
                dialog.saved()
            } else {
                // Never leave an orphaned file behind when the record
                // cannot be created (PRD F-07).
                Attachments.removeManagedFile(relativePath)
                dialog.pickedError = SnippetService.lastError
            }
        }
    }

    onOpened: {
        if (dialog.step === "type")
            typeTextTile.forceActiveFocus()
    }

    // An outside click / Esc closes the popup through closePolicy first. If
    // that would lose input (or interrupt an import), keep the dialog up and
    // show the discard confirmation instead. Re-opening does not re-run the
    // reset below, so the draft is preserved.
    onClosed: {
        if (importing || (step === "form" && !confirmingDiscard && isDirty())) {
            confirmingDiscard = true
            open()
            return
        }
        confirmingDiscard = false
        importing = false
        pendingToken = ""
        pickedUrl = ""
        pickedError = ""
        contentArea.text = ""
        displayNameField.text = ""
        validationLabel.visible = false
    }

    // ---------- small internal components ----------

    component TypeTile: Rectangle {
        id: tile

        property string label: ""
        property string iconSource: ""
        signal chosen()

        /// True only while the keyboard focus ring should be painted.
        readonly property bool focusOutline: tile.activeFocus && dialog.keyboardFocusVisible

        Layout.fillWidth: true
        implicitHeight: 96
        radius: Theme.radiusMedium
        color: tileArea.containsMouse ? "#F2F6FD" : "#FBFCFE"
        border.width: 1
        border.color: tile.focusOutline ? Theme.focusBorder
                      : tileArea.containsMouse ? Theme.borderStrong
                      : Theme.border
        activeFocusOnTab: true

        Behavior on color {
            ColorAnimation { duration: Theme.animationFast }
        }

        Column {
            anchors.centerIn: parent
            spacing: 8

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 26
                height: 26
                smooth: true
                fillMode: Image.PreserveAspectFit
                source: tile.iconSource
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: tile.label
                color: Theme.textPrimary
                font.pixelSize: 13
                font.weight: Font.Medium
            }
        }

        MouseArea {
            id: tileArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                // Mouse selection: no keyboard focus ring.
                dialog.keyboardFocusVisible = false
                tile.chosen()
            }
        }

        Keys.onReturnPressed: {
            dialog.keyboardFocusVisible = true
            tile.chosen()
        }
        Keys.onSpacePressed: {
            dialog.keyboardFocusVisible = true
            tile.chosen()
        }
        // Tab / Shift+Tab arrive here before the focus chain moves on, which
        // is the first reliable sign of keyboard navigation.
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab)
                dialog.keyboardFocusVisible = true
        }
    }

    // ---------- dialog content ----------

    contentItem: ColumnLayout {
        spacing: 14

        Keys.onEscapePressed: dialog.requestClose()

        // Keyboard navigation (Tab / arrows) turns the tile outline on; the
        // outline is not shown for the initial focus when the dialog opens.
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab
                    || event.key === Qt.Key_Left || event.key === Qt.Key_Right
                    || event.key === Qt.Key_Up || event.key === Qt.Key_Down)
                dialog.keyboardFocusVisible = true
        }

        Text {
            text: dialog.step === "type" ? qsTr("新建内容")
                  : dialog.contentType === "text" ? qsTr("新建文本话术")
                  : dialog.contentType === "image" ? qsTr("新建图片")
                  : qsTr("新建 PDF")
            color: Theme.textPrimary
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        // ----- step 1: content type -----
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: dialog.step === "type"

            TypeTile {
                id: typeTextTile
                objectName: "typeTextTile"
                label: qsTr("文本")
                iconSource: "../icons/text-lines-gray.svg"
                onChosen: dialog.chooseType("text")
            }

            TypeTile {
                objectName: "typeImageTile"
                label: qsTr("图片")
                iconSource: "../icons/image-gray.svg"
                onChosen: dialog.chooseType("image")
            }

            TypeTile {
                objectName: "typePdfTile"
                label: "PDF"
                iconSource: "../icons/pdf-gray.svg"
                onChosen: dialog.chooseType("pdf")
            }
        }

        // ----- step 2: text form -----
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: dialog.step === "form" && dialog.contentType === "text"

            TextArea {
                id: contentArea
                objectName: "newSnippetContentArea"
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
        }

        // ----- step 2: attachment form -----
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: dialog.step === "form" && dialog.contentType !== "text"

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                AppButton {
                    id: pickFileButton
                    objectName: "pickFileButton"
                    text: dialog.pickedUrl === "" ? qsTr("选择文件…") : qsTr("重新选择…")
                    variant: "secondary"
                    enabled: !dialog.importing
                    onClicked: dialog.pickFile()
                }

                Text {
                    Layout.fillWidth: true
                    text: dialog.pickedSizeText()
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                visible: dialog.contentType === "image"
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                radius: Theme.radiusMedium
                color: "#F4F7FC"
                border.width: 1
                border.color: "#E1E8F1"

                Image {
                    anchors.fill: parent
                    anchors.margins: 6
                    source: dialog.pickedUrl
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    sourceSize.width: 600
                    sourceSize.height: 400
                    visible: status === Image.Ready
                }

                Text {
                    anchors.centerIn: parent
                    visible: dialog.pickedUrl === ""
                    text: qsTr("选择图片后在这里预览")
                    color: Theme.textMuted
                    font.pixelSize: 12
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: qsTr("显示文件名")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }

                TextField {
                    id: displayNameField
                    Layout.fillWidth: true
                    implicitHeight: 38
                    placeholderText: dialog.contentType === "pdf" ? qsTr("例如：产品说明书.pdf")
                                                                  : qsTr("例如：门店海报.png")
                    placeholderTextColor: "#9BA8BA"
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    selectByMouse: true
                    leftPadding: 12
                    rightPadding: 12
                    verticalAlignment: TextInput.AlignVCenter
                    enabled: !dialog.importing

                    background: Rectangle {
                        radius: Theme.radiusMedium
                        color: "#FBFCFE"
                        border.width: 1
                        border.color: displayNameField.activeFocus ? Theme.focusBorder : Theme.border
                        Behavior on border.color {
                            ColorAnimation { duration: Theme.animationFast }
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: dialog.contentType === "pdf"
                      ? qsTr("仅支持 PDF 文件（≤100 MB），保存后用系统默认阅读器打开。")
                      : qsTr("支持 PNG / JPG / JPEG / WEBP / BMP（≤50 MB），文件会复制到 TalkBOX 附件目录。")
                color: Theme.textMuted
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            Text {
                id: pickedErrorLabel
                Layout.fillWidth: true
                visible: dialog.pickedError !== ""
                text: dialog.pickedError
                color: Theme.danger
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
        }

        // ----- category picker (both forms) -----
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: dialog.step === "form"

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
                objectName: "newCategoryHint"
                Layout.fillWidth: true
                visible: Categories.totalCount === 0
                text: qsTr("还没有分类：可在「设置 → 分类管理」中新建，也可以先不分类保存，之后再修改。")
                color: Theme.textMuted
                font.pixelSize: 12
                wrapMode: Text.Wrap
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

        // ----- discard confirmation -----
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: dialog.confirmingDiscard

            Text {
                Layout.fillWidth: true
                text: qsTr("放弃已填写的内容？")
                color: Theme.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("当前输入尚未保存，关闭后不会保留。")
                color: Theme.textSecondary
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }
        }

        // ----- footer -----
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "dialogBackButton"
                text: qsTr("返回")
                variant: "secondary"
                visible: dialog.step === "form" && !dialog.confirmingDiscard
                enabled: !dialog.importing
                onClicked: dialog.backToType()
            }

            AppButton {
                objectName: "dialogContinueEditingButton"
                text: qsTr("继续编辑")
                variant: "secondary"
                visible: dialog.confirmingDiscard
                onClicked: dialog.confirmingDiscard = false
            }

            AppButton {
                objectName: "dialogCancelButton"
                text: qsTr("取消")
                variant: "secondary"
                visible: !dialog.confirmingDiscard
                enabled: !dialog.importing
                onClicked: dialog.requestClose()
            }

            AppButton {
                objectName: "dialogSaveButton"
                text: dialog.importing ? qsTr("导入中…") : qsTr("保存")
                variant: "primary"
                visible: dialog.step === "form" && !dialog.confirmingDiscard
                enabled: !dialog.importing
                onClicked: dialog.trySave()
            }

            AppButton {
                objectName: "dialogDiscardButton"
                text: qsTr("放弃")
                variant: "danger"
                visible: dialog.confirmingDiscard
                onClicked: dialog.close()
            }
        }
    }
}
