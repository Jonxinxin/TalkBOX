import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import TalkBox

ApplicationWindow {
    id: window

    // Compact companion panel: the app is a small helper for replying to
    // messages, not a full-screen window.
    width: 420
    height: Math.min(800, Screen.desktopAvailableHeight - 48)
    minimumWidth: 400
    minimumHeight: 480
    /// Shown by main.cpp once WindowChrome has prepared the native frame, so
    /// the window never appears with a system caption for one frame.
    visible: false
    title: qsTr("TalkBOX · 话术库")

    /// The window keeps its native frame (WS_CAPTION | WS_THICKFRAME): DWM
    /// draws the drop shadow, Windows owns the resize edges, Aero snap, the
    /// system menu and the window animations. Only the caption is hidden and
    /// replaced by the QML TitleBar below (see src/platform/WindowChrome.cpp).
    /// Do not add Qt.FramelessWindowHint here: that would drop the native
    /// frame and with it the system shadow.
    color: Theme.pageBackground

    /// Keeps the panel above other windows (e.g. WeChat).
    property bool pinned: false

    /// Drag feedback pill: visible while a card is being dragged. The
    /// platform drag only changes the cursor, so the window itself shows
    /// what is being carried and where it can be dropped.
    property bool dragHintVisible: false
    property string dragHintTitle: ""
    property string dragHintDetail: ""

    /// Keep every native frame hint: the QML title bar sits on top of the
    /// native frame rather than replacing it.
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint
           | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint
           | Qt.WindowCloseButtonHint | (window.pinned ? Qt.WindowStaysOnTopHint : 0)

    Component.onCompleted: {
        const state = AppInfo.loadWindowState()
        if (state.width !== undefined) {
            window.width = Math.max(window.minimumWidth,
                                    Math.min(state.width, Screen.desktopAvailableWidth - 20))
            window.height = Math.max(window.minimumHeight,
                                     Math.min(state.height, Screen.desktopAvailableHeight - 20))
            window.x = Math.max(0, Math.min(state.x, Screen.desktopAvailableWidth - window.width))
            window.y = Math.max(0, Math.min(state.y, Screen.desktopAvailableHeight - window.height))
            window.pinned = state.pinned === true
        } else {
            // First run: dock near the right edge like a side helper panel.
            window.x = Math.max(0, Screen.desktopAvailableWidth - window.width - 24)
            window.y = 72
        }
        window.normalGeometry = Qt.rect(window.x, window.y, window.width, window.height)
    }

    Timer {
        id: windowStateTimer
        interval: 600
        onTriggered: window.persistWindowState()
    }

    /// Last geometry seen while the window was actually floating. Maximized /
    /// minimized windows report frame geometry (or -32000 while minimized),
    /// which must never overwrite the normal size / position.
    property rect normalGeometry: Qt.rect(0, 0, 0, 0)

    function rememberGeometry() {
        // Only a normally visible window has meaningful geometry.
        if (window.visibility !== Window.Windowed)
            return
        window.normalGeometry = Qt.rect(window.x, window.y, window.width, window.height)
        windowStateTimer.restart()
    }

    function persistWindowState() {
        if (window.visibility === Window.Windowed) {
            AppInfo.saveWindowState(window.x, window.y, window.width, window.height,
                                    window.pinned)
        } else if (window.normalGeometry.width > 0) {
            AppInfo.saveWindowState(window.normalGeometry.x, window.normalGeometry.y,
                                    window.normalGeometry.width, window.normalGeometry.height,
                                    window.pinned)
        }
    }

    onXChanged: window.rememberGeometry()
    onYChanged: window.rememberGeometry()
    onWidthChanged: window.rememberGeometry()
    onHeightChanged: window.rememberGeometry()
    onPinnedChanged: windowStateTimer.restart()

    onClosing: function(close) {
        window.persistWindowState()
        close.accepted = true
    }

    function filtersActive() {
        return searchField.text.trim() !== ""
                || Snippets.categoryFilter !== "all"
                || Snippets.typeFilter !== "all"
    }

    function resultLabel() {
        var typeLabel = Snippets.typeFilter === "image" ? qsTr("图片")
                : Snippets.typeFilter === "pdf" ? "PDF"
                : qsTr("全部内容")
        return typeLabel + " · " + Snippets.totalCount
    }

    function clearFilters() {
        searchField.clear()
        Snippets.searchQuery = ""
        Snippets.categoryFilter = "all"
        Snippets.typeFilter = "all"
    }

    function copySnippet(id) {
        if (SnippetService.copySnippet(id))
            toast.show(qsTr("话术已复制"), false)
        else
            toast.show(SnippetService.lastError, true)
    }

    function copyImage(absolutePath) {
        if (SystemService.copyImageToClipboard(absolutePath))
            toast.show(qsTr("图片已复制"), false)
        else
            toast.show(SystemService.lastError, true)
    }

    function copyFile(absolutePath) {
        if (SystemService.copyFileToClipboard(absolutePath))
            toast.show(qsTr("文件已复制，可粘贴到聊天窗口"), false)
        else
            toast.show(SystemService.lastError, true)
    }

    function openFile(absolutePath) {
        if (SystemService.openPath(absolutePath))
            toast.show(qsTr("已用系统默认程序打开"), false)
        else
            toast.show(SystemService.lastError, true)
    }

    function revealFile(absolutePath) {
        if (!SystemService.revealInFolder(absolutePath))
            toast.show(SystemService.lastError, true)
    }

    function showDragHint(label, snippetType) {
        dragHintTitle = snippetType === "text"
                ? qsTr("松开即可把文字拖到聊天窗口")
                : qsTr("松开即可把文件拖到聊天窗口")
        dragHintDetail = label
        dragHintVisible = true
    }

    function hideDragHint() {
        dragHintVisible = false
    }

    // Card body behaviour: always copy, whatever the type (F-06). Text is
    // copied as text, images as a bitmap and PDFs as a file reference.
    // Previewing / opening is now a menu action.
    function activateItem(item) {
        if (!item || !item.id)
            return
        if (item.type === "text")
            window.copySnippet(item.id)
        else if (item.type === "image")
            window.copyImage(item.absolutePath)
        else
            window.copyFile(item.absolutePath)
    }

    function activateCurrentItem() {
        window.activateItem(Snippets.itemAt(grid.currentIndex))
    }

    Shortcut {
        sequence: "Ctrl+N"
        enabled: !newSnippetDialog.visible
        onActivated: newSnippetDialog.openForCreate(true)
    }

    Shortcut {
        sequence: "Ctrl+K"
        enabled: !newSnippetDialog.visible
        onActivated: searchField.activate()
    }

    Shortcut {
        sequence: "Ctrl+F"
        enabled: !newSnippetDialog.visible
        onActivated: searchField.activate()
    }

    Timer {
        id: searchDebounce
        interval: 150
        onTriggered: Snippets.searchQuery = searchField.text
    }

    // Custom caption on top of the native frame. It is a sibling of the
    // content column (not part of the layout) so the content keeps its
    // original margins; only the top margin grows by the caption height.
    TitleBar {
        id: titleBar
        objectName: "titleBar"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: captionHeight
        z: 10
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: titleBar.height + 8
        anchors.bottomMargin: 10
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            SearchField {
                id: searchField
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.controlHeight
                onTextChanged: searchDebounce.restart()
            }

            Button {
                id: newButton
                objectName: "newButton"
                Layout.preferredHeight: Theme.controlHeight
                leftPadding: 14
                rightPadding: 14
                text: qsTr("新建")
                hoverEnabled: true
                // Caption-adjacent actions: never grab keyboard focus, so a
                // click cannot leave a focus ring behind.
                focusPolicy: Qt.NoFocus
                onClicked: newSnippetDialog.openForCreate()

                background: Rectangle {
                    radius: Theme.radiusMedium
                    color: newButton.down ? Theme.primaryPressed
                            : newButton.hovered ? Theme.primaryHover
                            : Theme.primary
                    Behavior on color {
                        ColorAnimation { duration: Theme.animationFast }
                    }
                }

                contentItem: Row {
                    spacing: 5

                    Image {
                        width: 14
                        height: 14
                        source: "icons/plus.svg"
                        smooth: true
                        fillMode: Image.PreserveAspectFit
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: newButton.text
                        color: Theme.textOnPrimary
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            IconButton {
                id: pinButton
                objectName: "pinButton"
                Layout.leftMargin: 2
                iconSource: Qt.resolvedUrl("icons/pin-gray.svg")
                activeIconSource: Qt.resolvedUrl("icons/pin-blue.svg")
                iconSize: 15
                toggled: window.pinned
                onClicked: window.pinned = !window.pinned
            }

            IconButton {
                id: settingsButton
                objectName: "settingsButton"
                iconSource: Qt.resolvedUrl("icons/settings.svg")
                iconSize: 16
                onClicked: settingsDialog.openDialog("backup")
            }
        }

        Flickable {
            id: categoriesScroll
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.preferredHeight: 30
            contentWidth: categoryChips.implicitWidth
            contentHeight: height
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            Row {
                id: categoryChips
                height: parent.height
                spacing: 6

                CategoryChip {
                    chipText: qsTr("全部")
                    categoryKey: "all"
                    selected: Snippets.categoryFilter === "all"
                    onClicked: Snippets.categoryFilter = "all"
                }

                Repeater {
                    model: Categories

                    CategoryChip {
                        objectName: "categoryChip"
                        chipText: model.name
                        categoryKey: model.colorKey
                        selected: Snippets.categoryFilter === model.id
                        onClicked: Snippets.categoryFilter = model.id
                    }
                }

                // Virtual entry: snippets without a category. Only shown when
                // such snippets exist (there are no built-in categories, so a
                // fresh library has nothing to show here).
                CategoryChip {
                    objectName: "uncategorizedChip"
                    chipText: qsTr("未分类")
                    categoryKey: "slate"
                    visible: Snippets.uncategorizedCount > 0
                    selected: Snippets.categoryFilter === "uncategorized"
                    onClicked: Snippets.categoryFilter = "uncategorized"
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Breathing room between the category tabs and the first card row.
            Layout.topMargin: 14

            GridView {
                id: grid
                objectName: "snippetGrid"
                anchors.fill: parent
                visible: Snippets.totalCount > 0
                clip: true
                // The view owns focus so Enter / Space activate the current
                // card, but that alone must not outline the first card.
                focus: true
                model: Snippets

                /// True only after an arrow / home / end / page key moved the
                /// current index. Mouse interaction clears it again, so the
                /// card outline means "keyboard position", not "view focused".
                property bool keyboardNavigation: false

                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Up || event.key === Qt.Key_Down
                            || event.key === Qt.Key_Left || event.key === Qt.Key_Right
                            || event.key === Qt.Key_Home || event.key === Qt.Key_End
                            || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown)
                        keyboardNavigation = true
                }

                // Clicking the search field or a category chip moves focus away;
                // the keyboard outline must not survive that.
                onActiveFocusChanged: {
                    if (!activeFocus)
                        keyboardNavigation = false
                }

                // No rubber-band bounce at the first / last row: the list
                // simply stops.
                boundsBehavior: Flickable.StopAtBounds

                readonly property int gap: 8
                // Room around the cards. A card's border is drawn centred on
                // its edge and grows to 2px while dragging, so the grid must
                // leave that much space at its left/top edge or the clipping
                // view would cut the outline off.
                readonly property int padding: 4
                // Two columns even in the compact panel; one only if the
                // window is made extremely narrow.
                readonly property int columns: width >= 900 - padding * 2 ? 3
                                               : (width - padding * 2 >= 360 ? 2 : 1)
                // Qt derives the column count from viewWidth / cellWidth.
                // cellWidth must therefore represent the full column pitch
                // (card + gap); the visual gap comes from the delegate
                // being narrower than the cell.
                readonly property int columnPitch:
                    Math.max(1, Math.floor((width - padding * 2) / columns))

                // Symmetric horizontal margins: inset the content area rather
                // than shifting each card.
                leftMargin: padding
                rightMargin: padding
                topMargin: padding

                cellWidth: columnPitch
                cellHeight: 96 + gap

                /// Extra width the last column needs so its right margin
                /// matches the left one after integer rounding.
                readonly property real lastColumnExtra:
                    Math.max(0, width - padding * 2
                             - columnPitch * (columns - 1) - (columnPitch - gap))

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AlwaysOff
                }

                Keys.onReturnPressed: window.activateCurrentItem()
                Keys.onEnterPressed: window.activateCurrentItem()
                Keys.onSpacePressed: window.activateCurrentItem()

                delegate: SnippetCard {
                    // Normal cards fill their cell except for the gap; the
                    // last column also absorbs the pixels lost to integer
                    // rounding, so the right margin matches the left one.
                    width: grid.columnPitch - grid.gap
                           + ((index % grid.columns) === grid.columns - 1
                              ? grid.lastColumnExtra : 0)
                    height: 96
                    snippetId: model.id
                    snippetType: model.type
                    content: model.content
                    previewText: model.preview
                    displayName: model.displayName
                    categoryName: model.categoryName
                    categoryColorKey: model.categoryColorKey
                    mimeType: model.mimeType
                    absolutePath: model.absolutePath
                    absoluteUrl: model.absoluteUrl
                    fileSize: model.fileSize

                    onDragStarted: function(label) {
                        window.showDragHint(label, snippetType)
                    }
                    onDragFinished: window.hideDragHint()

                    onCopyRequested: function(id) { window.copySnippet(id) }
                    onCopyImageRequested: function(id, absolutePath) {
                        window.copyImage(absolutePath)
                    }
                    onCopyFileRequested: function(id, absolutePath) {
                        window.copyFile(absolutePath)
                    }
                    onPreviewRequested: function(id, absoluteUrl, displayName, absolutePath) {
                        imagePreviewDialog.openFor(id, absoluteUrl, displayName, absolutePath)
                    }
                    onOpenRequested: function(id, absolutePath) { window.openFile(absolutePath) }
                    onEditRequested: function(id, content, categoryId) {
                        editTextDialog.openFor(id, content, categoryId)
                    }
                    onRenameRequested: function(id, displayName, snippetType) {
                        renameDialog.openFor(id, displayName, snippetType)
                    }
                    onChangeCategoryRequested: function(id, categoryId) {
                        changeCategoryDialog.openFor(id, categoryId)
                    }
                    onRevealRequested: function(absolutePath) { window.revealFile(absolutePath) }
                    onDeleteRequested: function(id, snippetType, describe) {
                        deleteDialog.openFor(id, snippetType, describe)
                    }
                }
            }

            EmptyState {
                anchors.fill: parent
                visible: Snippets.totalCount === 0
                title: window.filtersActive() ? qsTr("没有找到匹配的内容") : qsTr("还没有话术")
                subtitle: window.filtersActive()
                        ? qsTr("试着修改关键词，或切换分类与类型筛选。")
                        : qsTr("点击卡片复制内容，拖动可直接拖到聊天窗口；右键或点“…”打开菜单。")
                actionVisible: true
                actionText: window.filtersActive() ? qsTr("清除筛选") : qsTr("新建话术")
                onActionClicked: {
                    if (window.filtersActive())
                        window.clearFilters()
                    else
                        newSnippetDialog.openForCreate()
                }
            }
        }

        // Type filter (全部内容 / 图片 / PDF) at the bottom of the window:
        // the result count is left-aligned, the filter buttons right-aligned.
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 10
            Layout.bottomMargin: 2
            spacing: 8

            Text {
                objectName: "resultLabel"
                text: window.resultLabel()
                color: Theme.textMuted
                font.pixelSize: 13
                verticalAlignment: Text.AlignVCenter
            }

            Item {
                Layout.fillWidth: true
            }

            Row {
                id: typePillsRow
                spacing: 6

                TypeFilterButton {
                    objectName: "typeFilterAll"
                    filterKey: "all"
                    label: qsTr("全部内容")
                    iconName: "grid"
                }

                TypeFilterButton {
                    objectName: "typeFilterImage"
                    filterKey: "image"
                    label: qsTr("图片")
                    iconName: "image"
                }

                TypeFilterButton {
                    objectName: "typeFilterPdf"
                    filterKey: "pdf"
                    label: "PDF"
                    iconName: "pdf"
                }
            }
        }
    }

    ToastBar {
        id: toast
        objectName: "toastBar"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 26
        z: 50
    }

    // Drag feedback: a platform drag only changes the cursor, so while a
    // card is carried this pill names the content and the drop target.
    Rectangle {
        id: dragHint
        objectName: "dragHint"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 74
        width: Math.min(window.width - 32, 340)
        height: dragHintColumn.implicitHeight + 20
        radius: Theme.radiusMedium
        color: Theme.cardBackground
        border.width: 2
        border.color: Theme.primary
        visible: opacity > 0
        opacity: window.dragHintVisible ? 1 : 0
        z: 60

        Behavior on opacity {
            NumberAnimation { duration: 120 }
        }

        Column {
            id: dragHintColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3

            Text {
                width: parent.width
                text: window.dragHintTitle
                color: Theme.primary
                font.pixelSize: 12
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                width: parent.width
                visible: window.dragHintDetail !== ""
                text: window.dragHintDetail
                color: Theme.textSecondary
                font.pixelSize: 12
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    NewSnippetDialog {
        id: newSnippetDialog
        onSaved: toast.show(qsTr("已保存"), false)
    }

    EditTextDialog {
        id: editTextDialog
        onSaved: toast.show(qsTr("已保存"), false)
    }

    RenameDialog {
        id: renameDialog
        onRenamed: toast.show(qsTr("已重命名"), false)
    }

    ChangeCategoryDialog {
        id: changeCategoryDialog
        onChanged: toast.show(qsTr("分类已修改"), false)
    }

    DeleteConfirmDialog {
        id: deleteDialog
        onConfirmed: {
            if (SnippetService.deleteSnippet(deleteDialog.snippetId))
                toast.show(qsTr("已删除"), false)
            else
                toast.show(SnippetService.lastError, true)
        }
    }

    ImagePreviewDialog {
        id: imagePreviewDialog
        onCopied: toast.show(qsTr("图片已复制"), false)
    }

    Connections {
        target: SnippetService

        function onWarningOccurred(message) {
            toast.show(message, true)
        }
    }

    SettingsDialog {
        id: settingsDialog
    }

    Connections {
        target: BackupService

        function onBackupFinished(ok, error, path) {
            if (ok)
                toast.show(qsTr("备份已创建"), false)
            else
                toast.show(error, true)
        }

        function onRestoreFinished(ok, error, safetyCopyPath) {
            if (ok)
                toast.show(qsTr("恢复完成"), false)
            else
                toast.show(error, true)
        }
    }

    // A deleted category must not stay selected as the active filter.
    Connections {
        target: CategoryService

        function onCategoriesChanged() {
            if (Snippets.categoryFilter !== "all"
                    && !Categories.hasCategory(Snippets.categoryFilter)) {
                Snippets.categoryFilter = "all"
            }
        }
    }
}
