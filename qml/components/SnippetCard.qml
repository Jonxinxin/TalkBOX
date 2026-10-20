import QtQuick
import QtQuick.Controls
import TalkBox

// Grid card for one snippet. The body click always copies the content
// (text as text, images as a bitmap, PDFs as a file reference); previewing,
// opening and management live in the card menu, which opens from the
// right mouse button or the three-dot button (PRD 4.4/4.5, F-06).
Rectangle {
    id: card
    objectName: "snippetCard"

    property string snippetId: ""
    property string snippetType: "text"
    property string content: ""
    property string previewText: ""
    property string displayName: ""
    property string categoryName: ""
    property string categoryColorKey: "slate"
    property string mimeType: ""
    property string absolutePath: ""
    property string absoluteUrl: ""
    property int fileSize: -1

    signal copyRequested(string id)
    signal copyImageRequested(string id, string absolutePath)
    signal copyFileRequested(string id, string absolutePath)
    signal previewRequested(string id, string absoluteUrl, string displayName, string absolutePath)
    signal openRequested(string id, string absolutePath)
    signal editRequested(string id, string content, string categoryId)
    signal renameRequested(string id, string displayName, string snippetType)
    signal changeCategoryRequested(string id, string categoryId)
    signal revealRequested(string absolutePath)
    signal deleteRequested(string id, string snippetType, string describe)

    /// Drag feedback (PRD F-06): the platform drag only changes the cursor,
    /// so the card reports the gesture to the window, which shows what is
    /// being carried and where it can be dropped.
    signal dragStarted(string label)
    signal dragFinished()

    readonly property bool isAttachment: snippetType === "image" || snippetType === "pdf"
    /// The owning GridView (attached properties are only valid on the
    /// delegate root, so resolve the view once here).
    readonly property var listView: GridView.view
    /// Keyboard highlight only while the list is being driven by the
    /// keyboard. Merely having focus (e.g. the view is focused on startup so
    /// Enter copies the current card) must not outline the first card.
    readonly property bool keyboardCurrent: card.listView
            && GridView.isCurrentItem && card.listView.activeFocus
            && card.listView.keyboardNavigation === true

    /// Snippets without a category (category_id IS NULL) have no joined
    /// colour; the neutral slate chip is used for them.
    readonly property string effectiveColorKey: categoryColorKey !== "" ? categoryColorKey
                                                                        : "slate"

    readonly property string describeText: snippetType === "text"
            ? content.split("\n")[0]
            : displayName

    /// Set while a drag is in progress: suppresses the click that would
    /// otherwise fire when the drop completes.
    property bool dragPerformed: false
    property point dragHotSpot: Qt.point(24, 24)

    /// True from the moment the platform drag starts until it finishes.
    readonly property bool dragging: bodyClick.drag.active

    /// Short content summary shown in the window-level drag hint.
    readonly property string dragLabel: snippetType === "text"
            ? content.split("\n")[0] : displayName

    /// Payload handed to the platform drag: text snippets as plain text,
    /// attachments as a file URL so chat apps can attach the file.
    function dragPayload() {
        if (snippetType === "text")
            return { "text/plain": content }
        return { "text/uri-list": absoluteUrl }
    }

    // The card is the drag source (visible - Qt uses it for the drag
    // image); an invisible follower tracks the gesture so the card itself
    // never moves inside the grid.
    Drag.active: bodyClick.drag.active
    Drag.dragType: Drag.Automatic
    Drag.supportedActions: Qt.CopyAction
    Drag.mimeData: card.dragPayload()
    Drag.hotSpot: card.dragHotSpot

    Drag.onDragStarted: card.dragStarted(card.dragLabel)

    Drag.onDragFinished: function(dropAction) {
        // Any completed drag must not be treated as a card click.
        if (dropAction === Qt.IgnoreAction || dropAction === Qt.CopyAction
                || dropAction === Qt.MoveAction || dropAction === Qt.LinkAction) {
            card.dragPerformed = true
        }
        dragGhost.x = 0
        dragGhost.y = 0
        card.dragFinished()
    }

    readonly property string sizeText: {
        if (fileSize <= 0)
            return ""
        if (fileSize < 1024)
            return fileSize + " B"
        if (fileSize < 1024 * 1024)
            return (fileSize / 1024).toFixed(0) + " KB"
        return (fileSize / (1024 * 1024)).toFixed(1) + " MB"
    }

    /// True while the card shows the emphasised outline: hover, keyboard
    /// position (see keyboardCurrent) or an in-progress drag.
    readonly property bool highlighted: card.dragging || bodyClick.containsMouse
            || card.keyboardCurrent

    radius: Theme.radiusMedium
    color: Theme.cardBackground
    // While dragging the card lifts: a stronger outline and a slight scale
    // tell the user the gesture is being carried by this card.
    border.width: card.dragging ? 2 : 1
    border.color: card.dragging ? Theme.primary
            : card.highlighted ? Theme.borderStrong
            : Theme.border
    scale: card.dragging ? 1.03 : 1.0

    Behavior on border.color {
        ColorAnimation { duration: Theme.animationFast }
    }

    Behavior on scale {
        NumberAnimation { duration: Theme.animationFast }
    }

    /// Body click copies the content so it can be pasted straight into a
    /// chat window: text as text, images as a bitmap, PDFs as a file
    /// reference. Preview / open are menu actions.
    function activate() {
        if (snippetType === "text")
            card.copyRequested(card.snippetId)
        else if (snippetType === "image")
            card.copyImageRequested(card.snippetId, card.absolutePath)
        else
            card.copyFileRequested(card.snippetId, card.absolutePath)
    }

    /// Menu anchor: the ⋯ button (top-right) or the right-click position.
    function openMenu(parentItem, x, y) {
        cardMenu.popup(parentItem, x, y)
    }

    // Invisible follower used as the MouseArea drag target.
    Item {
        id: dragGhost

        visible: false
        width: 1
        height: 1
        x: 0
        y: 0
    }

    MouseArea {
        id: bodyClick
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: card.dragging ? Qt.DragCopyCursor : Qt.PointingHandCursor
        drag.target: dragGhost
        drag.threshold: 10
        onPressed: function(mouse) {
            card.dragPerformed = false
            card.dragHotSpot = Qt.point(mouse.x, mouse.y)
            // Mouse interaction hands the "current card" indication back to
            // hover: the keyboard outline is only for keyboard navigation.
            if (card.listView)
                card.listView.keyboardNavigation = false
        }
        onClicked: {
            if (!card.dragPerformed)
                card.activate()
        }
    }

    Column {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 9
        anchors.rightMargin: 9
        anchors.topMargin: 8
        spacing: 5
        // Fade the carried content so the lifted outline reads as the drag.
        opacity: card.dragging ? 0.45 : 1

        Behavior on opacity {
            NumberAnimation { duration: Theme.animationFast }
        }

        // ---- top row: category chip + (spacer) + more button ----
        Item {
            width: parent.width
            height: 20

            Rectangle {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                radius: 6
                color: Theme.chipBackground(card.effectiveColorKey)
                implicitWidth: chipRow.implicitWidth + 14
                implicitHeight: 20

                Row {
                    id: chipRow
                    anchors.centerIn: parent
                    spacing: 6

                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: Theme.chipDot(card.effectiveColorKey)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        // Snippets without a category are labelled here too;
                        // categories are all user created, so an empty name
                        // simply means "未分类".
                        text: card.categoryName !== "" ? card.categoryName : qsTr("未分类")
                        color: Theme.chipForeground(card.effectiveColorKey)
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Rectangle {
                id: moreButton
                objectName: "cardMoreButton"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 24
                height: 22
                radius: 6
                color: moreArea.containsMouse ? "#F0F4FA" : "transparent"

                Image {
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                    smooth: true
                    fillMode: Image.PreserveAspectFit
                    source: "../icons/more.svg"
                }

                MouseArea {
                    id: moreArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: card.openMenu(moreButton, moreButton.width - cardMenu.width,
                                              moreButton.height + 4)
                }
            }
        }

        // ---- body: text preview or attachment row ----
        Text {
            visible: card.snippetType === "text"
            width: parent.width
            text: card.previewText
            color: Theme.textPrimary
            font.pixelSize: 13
            lineHeight: 1.4
            lineHeightMode: Text.ProportionalHeight
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }        Row {
            visible: card.isAttachment
            width: parent.width
            height: 48
            spacing: 9

            Rectangle {
                width: 48
                height: 48
                radius: 8
                color: "#F4F7FC"
                border.width: 1
                border.color: "#E1E8F1"

                Image {
                    id: thumbnail
                    visible: card.snippetType === "image"
                    anchors.fill: parent
                    anchors.margins: 1
                    source: card.snippetType === "image" ? card.absoluteUrl : ""
                    sourceSize.width: 104
                    sourceSize.height: 104
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: true
                }

                Image {
                    anchors.centerIn: parent
                    width: card.snippetType === "image" ? 20 : 22
                    height: width
                    smooth: true
                    fillMode: Image.PreserveAspectFit
                    visible: card.snippetType === "pdf"
                            || thumbnail.status !== Image.Ready
                    source: card.snippetType === "pdf"
                            ? "../icons/pdf-gray.svg" : "../icons/image-gray.svg"
                }
            }

            Column {
                width: parent.width - 57
                spacing: 3

                Text {
                    width: parent.width
                    text: card.displayName
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    wrapMode: Text.WrapAnywhere
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }

                Text {
                    width: parent.width
                    color: (card.snippetType === "image" && thumbnail.status === Image.Error)
                           ? Theme.danger : Theme.textMuted
                    font.pixelSize: 11
                    text: {
                        if (card.snippetType === "pdf")
                            return card.sizeText === "" ? qsTr("PDF")
                                                        : qsTr("PDF · %1").arg(card.sizeText)
                        var base = qsTr("图片")
                        if (thumbnail.status === Image.Error)
                            return base + qsTr(" · 文件缺失")
                        return card.sizeText === "" ? base : base + " · " + card.sizeText
                    }
                    elide: Text.ElideRight
                }
            }
        }
    }

    // Context click: the same menu as the ⋯ button. The body area above
    // only accepts the left button, so right-clicking never copies or
    // starts a drag (PRD 4.5).
    MouseArea {
        id: contextClick
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: function(mouse) {
            card.openMenu(card, mouse.x, mouse.y)
        }
    }

    Menu {
        id: cardMenu
        objectName: "cardMenu"

        width: 176
        padding: 6

        background: Rectangle {
            radius: Theme.radiusMedium
            color: Theme.cardBackground
            border.width: 1
            border.color: "#E3EAF4"
        }

        // The body-click action first: copying is the most frequent task.
        CardMenuItem {
            objectName: "menuCopyContent"
            text: qsTr("复制内容")
            available: card.snippetType === "text"
            onTriggered: card.copyRequested(card.snippetId)
        }

        CardMenuItem {
            objectName: "menuCopyImage"
            text: qsTr("复制图片")
            available: card.snippetType === "image"
            onTriggered: card.copyImageRequested(card.snippetId, card.absolutePath)
        }

        CardMenuItem {
            objectName: "menuCopyFile"
            text: qsTr("复制文件")
            available: card.snippetType === "pdf"
            onTriggered: card.copyFileRequested(card.snippetId, card.absolutePath)
        }

        CardMenuItem {
            objectName: "menuPreviewImage"
            text: qsTr("预览")
            available: card.snippetType === "image"
            onTriggered: card.previewRequested(card.snippetId, card.absoluteUrl,
                                               card.displayName, card.absolutePath)
        }

        CardMenuItem {
            objectName: "menuOpenPdf"
            text: qsTr("打开")
            available: card.snippetType === "pdf"
            onTriggered: card.openRequested(card.snippetId, card.absolutePath)
        }

        CardMenuItem {
            objectName: "menuEditContent"
            text: qsTr("编辑内容")
            available: card.snippetType === "text"
            onTriggered: card.editRequested(card.snippetId, card.content, card.categoryId)
        }

        CardMenuItem {
            objectName: "menuRename"
            text: qsTr("重命名")
            available: card.isAttachment
            onTriggered: card.renameRequested(card.snippetId, card.displayName, card.snippetType)
        }

        CardMenuItem {
            objectName: "menuChangeCategory"
            text: qsTr("修改分类")
            onTriggered: card.changeCategoryRequested(card.snippetId, card.categoryId)
        }

        CardMenuItem {
            objectName: "menuRevealInFolder"
            text: qsTr("在文件夹中显示")
            available: card.isAttachment
            onTriggered: card.revealRequested(card.absolutePath)
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 9
                color: "transparent"
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    x: 6
                    width: parent.width - 12
                    height: 1
                    color: Theme.border
                }
            }
        }

        CardMenuItem {
            objectName: "menuDelete"
            text: qsTr("删除")
            danger: true
            onTriggered: card.deleteRequested(card.snippetId, card.snippetType, card.describeText)
        }
    }
}
