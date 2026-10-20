import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import TalkBox

// Settings dialog: data backup/restore (F-11), custom category management
// (F-13) and the about/data-location page (F-12).
Popup {
    id: dialog
    objectName: "settingsDialog"

    property string section: "backup" // backup | categories | about
    property string resultMessage: ""
    property bool resultIsError: false
    property string pendingCategoryId: ""
    property string pendingRestorePath: ""
    property string categoryError: ""

    // Adaptive width: the app window can be a narrow side panel.
    width: Math.min(580, (Overlay.overlay ? Overlay.overlay.width : 620) - 28)
    modal: true
    focus: true
    // Esc or a click anywhere outside the panel dismisses it.
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

    function openDialog(sectionName) {
        section = sectionName || "backup"
        resultMessage = ""
        resultIsError = false
        categoryError = ""
        open()
    }

    // An outside click / Esc intends to dismiss the panel. A running
    // backup/restore must not be abandoned, so keep the panel up instead.
    function requestClose() {
        if (BackupService.busy) {
            open()
            return
        }
        close()
    }

    onClosed: {
        if (BackupService.busy)
            open()
    }

    function startBackupCreation(destination) {
        resultMessage = ""
        resultIsError = false
        BackupService.createBackup(destination)
    }

    FileDialog {
        id: saveBackupPicker
        objectName: "saveBackupDialog"
        title: qsTr("选择备份保存位置")
        fileMode: FileDialog.SaveFile
        defaultSuffix: BackupService.backupFileExtension()
        currentFile: BackupService.suggestedBackupFileUrl()
        nameFilters: [qsTr("TalkBOX 备份 (*.talkboxbackup)")]
        onAccepted: dialog.startBackupCreation(selectedFile)
    }

    FileDialog {
        id: openBackupPicker
        objectName: "openBackupDialog"
        title: qsTr("选择要恢复的备份文件")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("TalkBOX 备份 (*.talkboxbackup)"), qsTr("所有文件 (*)")]
        onAccepted: {
            dialog.pendingRestorePath = selectedFile.toString()
            restoreConfirm.openFor(qsTr("从备份恢复数据？"),
                                   qsTr("当前的全部话术、分类和附件将被备份文件中的内容替换。"),
                                   qsTr("恢复前会自动把当前数据完整保留到备份目录，恢复失败会自动还原。"),
                                   qsTr("恢复"), true)
        }
    }

    ConfirmDialog {
        id: restoreConfirm
        objectName: "restoreConfirmDialog"
        onConfirmed: {
            dialog.resultMessage = ""
            dialog.resultIsError = false
            BackupService.restoreBackup(dialog.pendingRestorePath)
        }
    }

    TextInputDialog {
        id: nameInput
        objectName: "categoryNameInputDialog"
        onSubmitted: function(text) {
            const ok = dialog.pendingCategoryId === ""
                    ? CategoryService.createCategory(text)
                    : CategoryService.renameCategory(dialog.pendingCategoryId, text)
            if (ok) {
                nameInput.close()
                dialog.categoryError = ""
            } else {
                nameInput.showError(CategoryService.lastError)
            }
        }
    }

    ConfirmDialog {
        id: deleteCategoryConfirm
        objectName: "deleteCategoryConfirmDialog"
        onConfirmed: {
            if (CategoryService.deleteCategory(dialog.pendingCategoryId))
                dialog.categoryError = ""
            else
                dialog.categoryError = CategoryService.lastError
        }
    }

    Connections {
        target: BackupService

        function onBackupFinished(ok, error, path) {
            dialog.resultIsError = !ok
            dialog.resultMessage = ok ? qsTr("备份已创建：%1").arg(path) : error
        }

        function onRestoreFinished(ok, error, safetyCopyPath) {
            dialog.resultIsError = !ok
            if (ok) {
                dialog.resultMessage = safetyCopyPath !== ""
                        ? qsTr("恢复完成。原数据已保留在：%1").arg(safetyCopyPath)
                        : qsTr("恢复完成。")
            } else {
                dialog.resultMessage = error
            }
        }
    }

    component SectionTab: Rectangle {
        id: tab

        property string label: ""
        property string key: ""

        readonly property bool selected: dialog.section === key
        // Same-hue transparent rest colour: avoids the grey hover flash.
        readonly property color hoverTint: "#EAF0FA"
        readonly property color restTint: Qt.rgba(hoverTint.r, hoverTint.g, hoverTint.b, 0)

        implicitWidth: tabLabel.implicitWidth + 26
        implicitHeight: 32
        radius: Theme.radiusMedium
        color: tab.selected ? Theme.primarySoft : (tabArea.containsMouse ? hoverTint : restTint)
        border.width: tab.selected ? 1 : 0
        border.color: Theme.primary

        Behavior on color {
            ColorAnimation { duration: Theme.animationFast }
        }

        Text {
            id: tabLabel
            anchors.centerIn: parent
            text: tab.label
            color: tab.selected ? Theme.primary : Theme.textSecondary
            font.pixelSize: 13
            font.weight: tab.selected ? Font.DemiBold : Font.Normal
        }

        MouseArea {
            id: tabArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: dialog.section = tab.key
        }
    }

    contentItem: ColumnLayout {
        spacing: 14

        Keys.onEscapePressed: dialog.close()

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("设置")
                color: Theme.textPrimary
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }

            Item {
                Layout.fillWidth: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            SectionTab {
                objectName: "backupSectionTab"
                label: qsTr("数据备份")
                key: "backup"
            }

            SectionTab {
                objectName: "categoriesSectionTab"
                label: qsTr("分类管理")
                key: "categories"
            }

            SectionTab {
                objectName: "aboutSectionTab"
                label: qsTr("关于")
                key: "about"
            }

            Item {
                Layout.fillWidth: true
            }
        }

        StackLayout {
            Layout.fillWidth: true
            // Keep the dialog usable even in a short window.
            Layout.preferredHeight: Math.min(356, (Overlay.overlay
                                                    ? Overlay.overlay.height : 700) - 240)
            currentIndex: dialog.section === "backup" ? 0
                          : dialog.section === "categories" ? 1
                          : 2

            // ---------------- backup & restore ----------------
            ColumnLayout {
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    text: qsTr("备份文件（.talkboxbackup）包含数据库、全部图片 / PDF 附件和版本清单，可在其他电脑上恢复。")
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    AppButton {
                        objectName: "createBackupButton"
                        text: qsTr("创建备份…")
                        variant: "primary"
                        enabled: !BackupService.busy
                        onClicked: saveBackupPicker.open()
                    }

                    AppButton {
                        objectName: "restoreBackupButton"
                        text: qsTr("从备份恢复…")
                        variant: "secondary"
                        enabled: !BackupService.busy
                        onClicked: openBackupPicker.open()
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    AppButton {
                        text: qsTr("打开备份目录")
                        variant: "secondary"
                        enabled: !BackupService.busy
                        onClicked: SystemService.openPath(BackupService.backupsDirectory())
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: progressColumn.implicitHeight + 24
                    radius: Theme.radiusMedium
                    color: "#F7FAFF"
                    border.width: 1
                    border.color: Theme.border
                    visible: BackupService.busy || dialog.resultMessage !== ""

                    ColumnLayout {
                        id: progressColumn
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        ProgressBar {
                            id: progressBar
                            Layout.fillWidth: true
                            visible: BackupService.busy
                            from: 0
                            to: 100
                            value: BackupService.progress

                            background: Rectangle {
                                implicitHeight: 6
                                radius: 3
                                color: "#E3EAF4"
                            }

                            contentItem: Item {
                                implicitHeight: 6

                                Rectangle {
                                    width: progressBar.visualPosition * parent.width
                                    height: parent.height
                                    radius: 3
                                    color: Theme.primary
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: BackupService.busy ? BackupService.statusText : dialog.resultMessage
                            color: dialog.resultIsError ? Theme.danger : Theme.textSecondary
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("恢复会覆盖当前数据；软件会先自动保留当前数据的完整副本，恢复失败时自动还原。")
                    color: Theme.textMuted
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }

                Item {
                    Layout.fillHeight: true
                }
            }

            // ---------------- category management ----------------
            ColumnLayout {
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("删除分类时，其中的内容会变成「未分类」，话术本身不会被删除。")
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                    }

                    AppButton {
                        objectName: "createCategoryButton"
                        text: qsTr("新建分类")
                        variant: "primary"
                        onClicked: {
                            dialog.pendingCategoryId = ""
                            nameInput.openFor(qsTr("新建分类"), qsTr("分类名称"),
                                              qsTr("例如：安装指导"), "")
                        }
                    }
                }

                ListView {
                    id: categoryList
                    objectName: "categoryList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: Categories

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AlwaysOff
                    }

                    // No built-in categories: a fresh install has none.
                    Text {
                        objectName: "categoryListEmptyHint"
                        anchors.centerIn: parent
                        width: parent.width - 32
                        visible: Categories.totalCount === 0
                        text: qsTr("还没有分类。新建后即可在顶部筛选，并在新建内容时选用。")
                        color: Theme.textMuted
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 46
                        radius: Theme.radiusMedium
                        color: "#FBFCFE"
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 8

                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: Theme.chipDot(model.colorKey)
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Text {
                                Layout.fillWidth: true
                                text: model.name
                                color: Theme.textPrimary
                                font.pixelSize: 13
                                elide: Text.ElideRight
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Text {
                                text: CategoryService.snippetCount(model.id) + qsTr(" 条")
                                color: Theme.textMuted
                                font.pixelSize: 11
                                Layout.alignment: Qt.AlignVCenter
                            }

                            AppButton {
                                objectName: "renameCategoryButton"
                                text: qsTr("重命名")
                                variant: "secondary"
                                implicitHeight: 28
                                leftPadding: 10
                                rightPadding: 10
                                onClicked: {
                                    dialog.pendingCategoryId = model.id
                                    nameInput.openFor(qsTr("重命名分类"), qsTr("分类名称"),
                                                      qsTr("输入新的分类名称"), model.name)
                                }
                            }

                            AppButton {
                                objectName: "deleteCategoryButton"
                                text: qsTr("删除")
                                variant: "danger"
                                implicitHeight: 28
                                leftPadding: 10
                                rightPadding: 10
                                onClicked: {
                                    dialog.pendingCategoryId = model.id
                                    const count = CategoryService.snippetCount(model.id)
                                    deleteCategoryConfirm.openFor(
                                                qsTr("删除分类「%1」？").arg(model.name),
                                                qsTr("该分类下的 %1 条内容会变成「未分类」。").arg(count),
                                                qsTr("删除分类不会删除任何话术内容。"),
                                                qsTr("删除"), true)
                                }
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: dialog.categoryError !== ""
                    text: dialog.categoryError
                    color: Theme.danger
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }

            // ---------------- about ----------------
            ColumnLayout {
                spacing: 12

                ColumnLayout {
                    spacing: 4

                    Text {
                        text: qsTr("TalkBOX · 话术库")
                        color: Theme.textPrimary
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: qsTr("版本 %1 · 纯本地运行，不上传任何数据").arg(AppInfo.appVersion)
                        color: Theme.textSecondary
                        font.pixelSize: 12
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: qsTr("数据目录：")
                            color: Theme.textSecondary
                            font.pixelSize: 12
                        }

                        Text {
                            Layout.fillWidth: true
                            text: AppInfo.dataDirectory
                            color: Theme.textMuted
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                        }

                        AppButton {
                            text: qsTr("打开")
                            variant: "secondary"
                            implicitHeight: 28
                            leftPadding: 12
                            rightPadding: 12
                            onClicked: AppInfo.openDataDirectory()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: qsTr("日志目录：")
                            color: Theme.textSecondary
                            font.pixelSize: 12
                        }

                        Text {
                            Layout.fillWidth: true
                            text: AppInfo.logDirectory
                            color: Theme.textMuted
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                        }

                        AppButton {
                            text: qsTr("打开")
                            variant: "secondary"
                            implicitHeight: 28
                            leftPadding: 12
                            rightPadding: 12
                            onClicked: AppInfo.openLogDirectory()
                        }
                    }

                    Text {
                        text: qsTr("当前内容：%1 条 · 分类 %2 个")
                              .arg(Snippets.totalCount)
                              .arg(Categories.totalCount)
                        color: Theme.textMuted
                        font.pixelSize: 12
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("备份文件为 .talkboxbackup（ZIP 容器）：manifest.json 清单 + talkbox.db 数据库快照 + attachments 附件。")
                    color: Theme.textMuted
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "settingsCloseButton"
                text: qsTr("关闭")
                variant: "secondary"
                enabled: !BackupService.busy
                onClicked: dialog.requestClose()
            }
        }
    }
}
