
import sys
import json
import os
import shutil
import uuid
from pathlib import Path
from ctypes import windll

from PySide6.QtWidgets import (
    QApplication,
    QWidget,
    QVBoxLayout,
    QHBoxLayout,
    QFrame,
    QScrollArea,
    QDialog,
    QTextEdit,
    QMessageBox,
    QMenu,
    QLabel,
    QSizePolicy,
    QFileDialog,
    QPushButton,
    QListWidget,
    QListWidgetItem,
    QAbstractItemView
)

from PySide6.QtGui import QGuiApplication, QAction, QIcon, QDrag, QPixmap
from PySide6.QtCore import Qt, QTimer, QMimeData, QUrl, QSize

from qfluentwidgets import (
    SearchLineEdit,
    PushButton,
    PrimaryPushButton,
    CardWidget,
    BodyLabel,
    ToolButton,
    InfoBar,
    InfoBarPosition,
    FluentIcon as FIF,
    setTheme,
    Theme
)

APP_DATA_DIR = Path(os.getenv("APPDATA", Path.home())) / "Talkbox"
DATA_FILE = APP_DATA_DIR / "talk_data.json"
IMAGE_DIR = APP_DATA_DIR / "images"
PDF_DIR = APP_DATA_DIR / "pdfs"
LEGACY_DATA_FILE = Path("talk_data.json")
APP_ICON_FILE = "app.ico"
IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp"}
ALL_CATEGORIES = "全部"
UNCATEGORIZED = "未分类"
FIXED_CATEGORIES = ("开场", "电表", "报价", "售后")
CATEGORY_SELECT_OPTIONS = (UNCATEGORIZED, *FIXED_CATEGORIES)
CATEGORY_TAB_STYLE = """
    QPushButton {
        color: #475569;
        background: #ffffff;
        border: 1px solid #d7e2ef;
        border-radius: 8px;
        font-size: 12px;
        font-weight: 600;
    }

    QPushButton:hover {
        color: #2563eb;
        background: #f5f9ff;
        border: 1px solid #9ebff3;
    }

    QPushButton:checked {
        color: #ffffff;
        background: #2563eb;
        border: 1px solid #1d4ed8;
    }

    QPushButton:checked:hover {
        background: #1d4ed8;
        border: 1px solid #1e40af;
    }
"""


def resourcePath(fileName):

    basePath = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
    return basePath / fileName


class GlassFrame(QFrame):

    def __init__(self, parent=None):
        super().__init__(parent)

        self.setStyleSheet("""
            QFrame {
                background: #ffffff;
                border-radius: 8px;
                border: 1px solid #dce6f1;
            }
        """)


class DragOverlay(QFrame):

    def __init__(self, parent=None):
        super().__init__(parent)

        self.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents)

        layout = QVBoxLayout(self)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        label = QLabel("正在拖拽话术")
        label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        label.setStyleSheet("""
            QLabel {
                color: #ffffff;
                background: rgba(37, 99, 235, 0.92);
                border-radius: 8px;
                padding: 10px 14px;
                font-size: 14px;
                font-weight: 600;
            }
        """)

        layout.addWidget(label)

        self.setStyleSheet("""
            DragOverlay {
                background: rgba(15, 23, 42, 0.28);
                border: none;
            }
        """)


class CategoryButtonBar(QWidget):

    def __init__(self, options, selected=None, onChanged=None, parent=None):
        super().__init__(parent)

        self.options = tuple(options)
        self.selectedValue = selected if selected in self.options else self.options[0]
        self.onChanged = onChanged
        self.buttons = {}

        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)

        for option in self.options:
            button = QPushButton(option)
            button.setCheckable(True)
            button.setFixedSize(48, 28)
            button.setStyleSheet(CATEGORY_TAB_STYLE)
            button.clicked.connect(
                lambda checked=False, value=option: self.select(value)
            )

            self.buttons[option] = button
            layout.addWidget(button)

        layout.addStretch()
        self.updateButtons()

    def select(self, value):

        if value not in self.options:
            return

        self.selectedValue = value
        self.updateButtons()

        if self.onChanged:
            self.onChanged(value)

    def updateButtons(self):

        for value, button in self.buttons.items():
            button.setChecked(value == self.selectedValue)

    def value(self):

        return self.selectedValue


class CenteredDialog(QDialog):

    def showEvent(self, event):

        super().showEvent(event)
        self.moveToParentCenter()

    def moveToParentCenter(self):

        parent = self.parentWidget()

        if not parent:
            return

        parentWindow = parent.window()
        parentGeometry = parentWindow.frameGeometry()
        dialogGeometry = self.frameGeometry()

        dialogGeometry.moveCenter(parentGeometry.center())

        screen = parentWindow.screen()

        if screen:
            available = screen.availableGeometry()
            dialogGeometry.moveLeft(max(
                available.left(),
                min(dialogGeometry.left(), available.right() - dialogGeometry.width() + 1)
            ))
            dialogGeometry.moveTop(max(
                available.top(),
                min(dialogGeometry.top(), available.bottom() - dialogGeometry.height() + 1)
            ))

        self.move(dialogGeometry.topLeft())


class EditDialog(CenteredDialog):

    def __init__(self, parent=None, content="", category="", categories=None):
        super().__init__(parent)

        self.setWindowTitle("编辑话术")
        self.resize(280, 240)

        layout = QVBoxLayout(self)

        self.categoryBar = CategoryButtonBar(
            categories or CATEGORY_SELECT_OPTIONS,
            category if category else UNCATEGORIZED,
            parent=self
        )

        self.contentEdit = QTextEdit()
        self.contentEdit.setText(content)

        btnLayout = QHBoxLayout()

        saveBtn = PrimaryPushButton("保存")
        cancelBtn = PushButton("取消")

        saveBtn.clicked.connect(self.accept)
        cancelBtn.clicked.connect(self.reject)

        btnLayout.addStretch()
        btnLayout.addWidget(cancelBtn)
        btnLayout.addWidget(saveBtn)

        layout.addWidget(self.categoryBar)
        layout.addWidget(self.contentEdit)
        layout.addLayout(btnLayout)

    def getData(self):
        return {
            "content": self.contentEdit.toPlainText(),
            "category": self.categoryBar.value()
        }

class CategoryDialog(CenteredDialog):

    def __init__(self, parent=None, category=""):
        super().__init__(parent)

        self.setWindowTitle("设置分类")
        self.resize(280, 96)

        layout = QVBoxLayout(self)

        self.categoryBar = CategoryButtonBar(
            CATEGORY_SELECT_OPTIONS,
            category if category else UNCATEGORIZED,
            parent=self
        )

        btnLayout = QHBoxLayout()

        saveBtn = PrimaryPushButton("保存")
        cancelBtn = PushButton("取消")

        saveBtn.clicked.connect(self.accept)
        cancelBtn.clicked.connect(self.reject)

        btnLayout.addStretch()
        btnLayout.addWidget(cancelBtn)
        btnLayout.addWidget(saveBtn)

        layout.addWidget(self.categoryBar)
        layout.addLayout(btnLayout)

    def getCategory(self):

        return self.categoryBar.value()


class SortDialog(CenteredDialog):

    def __init__(self, page):
        super().__init__(page)

        self.talks = list(page.data)
        self.setWindowTitle("调整话术顺序")
        self.resize(340, 480)
        self.setMinimumSize(300, 340)

        layout = QVBoxLayout(self)
        hint = BodyLabel("在各组内拖动条目调整顺序，点击保存生效。\n置顶话术始终显示在普通话术前面。")
        hint.setWordWrap(True)
        layout.addWidget(hint)

        self.pinnedList = self.createList()
        self.normalList = self.createList()

        for index, talk in enumerate(self.talks):
            if page.isPdfTalk(talk):
                text = f"PDF · {page.pdfFilePath(talk).name}"
                icon = FIF.DOCUMENT.icon()
            elif page.isImageTalk(talk):
                imagePath = page.imageFilePath(talk)
                text = f"图片 · {imagePath.name}"
                icon = QIcon(str(imagePath))
            else:
                text = " ".join(talk.get("content", "").split())
                icon = FIF.EDIT.icon()

            category = page.talkCategory(talk)

            if category:
                text = f"{category} · {text}"

            item = QListWidgetItem(icon, text)
            item.setData(Qt.ItemDataRole.UserRole, index)
            item.setToolTip(page.talkTooltip(talk))
            item.setSizeHint(QSize(0, 40))
            item.setFlags(item.flags() & ~Qt.ItemFlag.ItemIsDropEnabled)
            target = self.pinnedList if page.isTalkPinned(talk) else self.normalList
            target.addItem(item)

        for title, listWidget in (("置顶话术", self.pinnedList), ("普通话术", self.normalList)):
            label = BodyLabel(f"{title}（{listWidget.count()}）")
            layout.addWidget(label)
            layout.addWidget(listWidget, 1)
            label.setVisible(listWidget.count() > 0)
            listWidget.setVisible(listWidget.count() > 0)

        buttons = QHBoxLayout()
        saveBtn = PrimaryPushButton("保存")
        cancelBtn = PushButton("取消")
        saveBtn.clicked.connect(self.accept)
        cancelBtn.clicked.connect(self.reject)
        buttons.addStretch()
        buttons.addWidget(cancelBtn)
        buttons.addWidget(saveBtn)
        layout.addLayout(buttons)

    def createList(self):

        listWidget = QListWidget(self)
        listWidget.setDragDropMode(QAbstractItemView.DragDropMode.InternalMove)
        listWidget.setDefaultDropAction(Qt.DropAction.MoveAction)
        listWidget.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        listWidget.setTextElideMode(Qt.TextElideMode.ElideRight)
        listWidget.setIconSize(QSize(24, 24))
        listWidget.setMinimumHeight(70)
        listWidget.setStyleSheet("""
            QListWidget {
                background: #ffffff;
                color: #1e293b;
                border: 1px solid #d7e2ef;
                border-radius: 8px;
                padding: 4px;
            }
            QListWidget::item { border-radius: 4px; padding: 4px; }
            QListWidget::item:selected { background: #dbeafe; color: #1d4ed8; }
            QListWidget::item:hover { background: #eff6ff; }
        """)
        return listWidget

    def orderedTalks(self):

        return [
            self.talks[listWidget.item(row).data(Qt.ItemDataRole.UserRole)]
            for listWidget in (self.pinnedList, self.normalList)
            for row in range(listWidget.count())
        ]


class TalkRow(CardWidget):

    ROW_HEIGHT = 72
    META_LABEL_WIDTH = 34
    META_LABEL_HEIGHT = 16
    META_LABEL_TOP = 4
    META_LABEL_LEFT = 10
    META_LABEL_GAP = 4

    def __init__(self, data, parentWindow):
        super().__init__()

        self.data = data
        self.parentWindow = parentWindow
        self.dragStartPosition = None
        self.isImage = self.parentWindow.isImageTalk(self.data)
        self.isPdf = self.parentWindow.isPdfTalk(self.data)

        self.setFixedHeight(self.ROW_HEIGHT)

        layout = QHBoxLayout(self)
        isPinned = self.parentWindow.isTalkPinned(self.data)
        category = self.parentWindow.talkCategory(self.data)

        if isPinned or category:
            layout.setContentsMargins(12, 18, 10, 5)
        else:
            layout.setContentsMargins(12, 7, 10, 7)

        metaX = self.META_LABEL_LEFT

        if isPinned:
            self.createMetaLabel(
                "置顶",
                metaX,
                "#815200",
                "#fff1c2",
                "#f4c95d"
            )
            metaX += self.META_LABEL_WIDTH + self.META_LABEL_GAP

        if category:
            self.createMetaLabel(
                category,
                metaX,
                "#2563eb",
                "#edf5ff",
                "#bfd7ff",
                category
            )

        layout.setSpacing(8)

        if self.isImage:
            imagePath = self.parentWindow.imageFilePath(self.data)
            self.fullText = f"图片 {imagePath.name}"

            thumbnail = QLabel()
            thumbnail.setFixedSize(36, 36)
            thumbnail.setAlignment(Qt.AlignmentFlag.AlignCenter)
            thumbnail.setStyleSheet("""
                QLabel {
                    background: #f1f5f9;
                    border: 1px solid #d7e2ef;
                    border-radius: 6px;
                }
            """)

            pixmap = QPixmap(str(imagePath))

            if pixmap.isNull():
                thumbnail.setText("图")
            else:
                thumbnail.setPixmap(
                    pixmap.scaled(
                        34,
                        34,
                        Qt.AspectRatioMode.KeepAspectRatio,
                        Qt.TransformationMode.SmoothTransformation
                    )
                )
        elif self.isPdf:
            self.fullText = self.parentWindow.pdfFilePath(self.data).name

            thumbnail = QLabel("PDF")
            thumbnail.setFixedSize(36, 36)
            thumbnail.setAlignment(Qt.AlignmentFlag.AlignCenter)
            thumbnail.setStyleSheet("""
                QLabel {
                    color: #dc2626;
                    background: #fef2f2;
                    border: 1px solid #fecaca;
                    border-radius: 6px;
                    font-size: 10px;
                    font-weight: 600;
                }
            """)
        else:
            self.fullText = data["content"].replace("\n", " ")

        self.textLabel = BodyLabel()
        self.textLabel.setMinimumWidth(0)
        self.textLabel.setWordWrap(False)
        self.textLabel.setAlignment(Qt.AlignmentFlag.AlignVCenter)
        self.textLabel.setStyleSheet("""
            QLabel {
                color: #1e293b;
                font-size: 13px;
            }
        """)
        self.textLabel.setSizePolicy(
            QSizePolicy.Policy.Expanding,
            QSizePolicy.Policy.Preferred
        )

        # 鼠标只悬停在文字上时显示完整话术
        self.textLabel.setToolTip(self.parentWindow.talkTooltip(self.data))

        editBtn = ToolButton(FIF.EDIT)
        copyBtn = ToolButton(FIF.COPY)

        editBtn.setFixedSize(30, 30)
        copyBtn.setFixedSize(30, 30)

        if self.isPdf:
            editBtn.setToolTip("替换 PDF")
            copyBtn.setToolTip("复制 PDF 文件")

        editBtn.clicked.connect(self.editTalk)
        copyBtn.clicked.connect(self.copyTalk)

        # 双击整行直接复制
        self.mouseDoubleClickEvent = self.doubleCopy


        if self.isImage or self.isPdf:
            layout.addWidget(thumbnail)

        layout.addWidget(self.textLabel, 1)
        layout.addWidget(editBtn)
        layout.addWidget(copyBtn)

        QTimer.singleShot(0, self.updateElidedText)

        self.setStyleSheet("""
            TalkRow {
                background: #ffffff;
                border-radius: 8px;
                border: 1px solid #dce6f1;
            }

            TalkRow:hover {
                background: #f8fbff;
                border: 1px solid #5b9dff;
                border-radius: 8px;
            }

            TalkRow:focus {
                border: 1px solid #2f7df6;
                border-radius: 8px;
            }
        """)

    def createMetaLabel(self, text, x, color, background, border, tooltip=""):

        label = QLabel(self)
        label.setFixedSize(self.META_LABEL_WIDTH, self.META_LABEL_HEIGHT)
        label.move(x, self.META_LABEL_TOP)
        label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        label.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents)
        label.setToolTip(tooltip)
        label.setText(
            label.fontMetrics().elidedText(
                text,
                Qt.TextElideMode.ElideRight,
                self.META_LABEL_WIDTH - 6
            )
        )
        label.setStyleSheet(f"""
            QLabel {{
                color: {color};
                background: {background};
                border: 1px solid {border};
                border-radius: 6px;
                font-size: 10px;
                font-weight: 600;
            }}
        """)
        label.raise_()
        return label

    def updateElidedText(self):

        width = max(20, self.textLabel.width() - 2)
        self.textLabel.setText(self.previewText(width))

    def previewText(self, width):

        metrics = self.textLabel.fontMetrics()
        text = self.fullText.strip()

        if metrics.horizontalAdvance(text) <= width:
            return text

        firstLineLength = self.fittedTextLength(text, width, metrics)

        if firstLineLength <= 0:
            return metrics.elidedText(
                text,
                Qt.TextElideMode.ElideRight,
                width
            )

        firstLine = text[:firstLineLength].rstrip()
        secondLine = text[firstLineLength:].lstrip()

        return "\n".join([
            firstLine,
            metrics.elidedText(
                secondLine,
                Qt.TextElideMode.ElideRight,
                width
            )
        ])

    def fittedTextLength(self, text, width, metrics):

        low = 0
        high = len(text)

        while low < high:
            mid = (low + high + 1) // 2

            if metrics.horizontalAdvance(text[:mid]) <= width:
                low = mid
            else:
                high = mid - 1

        return low

    def resizeEvent(self, event):

        super().resizeEvent(event)
        self.updateElidedText()

    def mousePressEvent(self, event):

        if event.button() == Qt.MouseButton.LeftButton:
            self.dragStartPosition = event.position().toPoint()

        super().mousePressEvent(event)

    def mouseMoveEvent(self, event):

        if not event.buttons() & Qt.MouseButton.LeftButton:
            return super().mouseMoveEvent(event)

        if self.dragStartPosition is None:
            return super().mouseMoveEvent(event)

        distance = (event.position().toPoint() - self.dragStartPosition).manhattanLength()

        if distance < QApplication.startDragDistance():
            return super().mouseMoveEvent(event)

        self.startTextDrag()

    def createDragMimeData(self):

        mimeData = QMimeData()

        if self.isImage:
            imagePath = self.parentWindow.imageFilePath(self.data)

            if not imagePath.exists():
                self.parentWindow.showWarning("图片不存在", "这张图片文件已经丢失")
                return

            mimeData.setUrls([QUrl.fromLocalFile(str(imagePath))])

            pixmap = QPixmap(str(imagePath))

            if not pixmap.isNull():
                mimeData.setImageData(pixmap.toImage())
        elif self.isPdf:
            pdfPath = self.parentWindow.pdfFilePath(self.data)

            if not pdfPath.is_file():
                self.parentWindow.showWarning("PDF 不存在", "PDF 文件已经丢失，请重新添加或替换")
                return None

            mimeData.setUrls([QUrl.fromLocalFile(str(pdfPath))])
        else:
            mimeData.setText(self.data["content"])

        return mimeData

    def startTextDrag(self):

        mimeData = self.createDragMimeData()

        if mimeData is None:
            return

        drag = QDrag(self)
        drag.setMimeData(mimeData)

        window = self.window()

        if hasattr(window, "showDragOverlay"):
            window.showDragOverlay()

        try:
            drag.exec(Qt.DropAction.CopyAction)
        finally:
            if hasattr(window, "hideDragOverlay"):
                window.hideDragOverlay()

    def contextMenuEvent(self, event):

        menu = QMenu(self)

        if self.parentWindow.isTalkPinned(self.data):
            pinAction = QAction("取消置顶", self)
            pinAction.triggered.connect(self.unpinTalk)
        else:
            pinAction = QAction("置顶话术", self)
            pinAction.triggered.connect(self.pinTalk)

        deleteAction = QAction("删除话术", self)
        categoryAction = QAction("设置分类", self)
        sortAction = QAction("调整顺序…", self)
        sortAction.setEnabled(len(self.parentWindow.data) > 1)

        categoryAction.triggered.connect(self.setTalkCategory)
        deleteAction.triggered.connect(self.deleteTalk)
        sortAction.triggered.connect(self.parentWindow.sortTalks)

        menu.addAction(pinAction)
        menu.addAction(categoryAction)
        menu.addAction(sortAction)
        menu.addSeparator()
        menu.addAction(deleteAction)

        menu.exec(event.globalPos())

    def copyTalk(self):

        if self.isPdf:
            self.copyPdfTalk()
            return

        if self.isImage:
            self.copyImageTalk()
            return

        QApplication.clipboard().setText(self.data["content"])

        InfoBar.success(
            title='复制成功',
            content='话术已复制到剪贴板',
            orient=Qt.Horizontal,
            isClosable=False,
            position=InfoBarPosition.BOTTOM,
            duration=1500,
            parent=self.parentWindow
        )

    def copyImageTalk(self):

        imagePath = self.parentWindow.imageFilePath(self.data)
        pixmap = QPixmap(str(imagePath))

        if pixmap.isNull():
            self.parentWindow.showWarning("图片不存在", "这张图片文件已经丢失")
            return

        QApplication.clipboard().setPixmap(pixmap)

        InfoBar.success(
            title='复制成功',
            content='图片已复制到剪贴板',
            orient=Qt.Horizontal,
            isClosable=False,
            position=InfoBarPosition.BOTTOM,
            duration=1500,
            parent=self.parentWindow
        )


    def copyPdfTalk(self):

        mimeData = self.createDragMimeData()

        if mimeData is None:
            return

        QApplication.clipboard().setMimeData(mimeData)
        self.parentWindow.showTip("复制成功", "PDF 文件已复制，可粘贴到聊天窗口")

    def doubleCopy(self, event):

        self.copyTalk()

    def editTalk(self):

        if self.isPdf:
            self.parentWindow.replacePdfTalk(self.data)
            return

        if self.isImage:
            self.parentWindow.replaceImageTalk(self.data)
            return

        dialog = EditDialog(
            self,
            self.data["content"],
            self.parentWindow.talkCategory(self.data),
            CATEGORY_SELECT_OPTIONS
        )

        if dialog.exec():

            data = dialog.getData()

            self.data["content"] = data["content"]
            self.parentWindow.setTalkCategoryValue(self.data, data["category"])

            self.parentWindow.saveData()
            self.parentWindow.refreshRows()

    def pinTalk(self):

        self.parentWindow.pinTalk(self.data)

    def unpinTalk(self):

        self.parentWindow.unpinTalk(self.data)

    def deleteTalk(self):

        self.parentWindow.deleteTalk(self.data)

    def setTalkCategory(self):

        self.parentWindow.setTalkCategory(self.data)


class HomePage(QWidget):

    def __init__(self, mainWindow):
        super().__init__()

        self.mainWindow = mainWindow

        self.data = self.loadData()
        self.data.sort(key=lambda talk: not self.isTalkPinned(talk))
        self.rows = []
        self.selectedCategory = ALL_CATEGORIES

        self.setObjectName("homePage")
        self.setStyleSheet("""
            QWidget#homePage {
                background: #eef4fa;
            }
        """)

        root = QVBoxLayout(self)

        root.setContentsMargins(10, 10, 10, 10)
        root.setSpacing(10)

        topBar = GlassFrame()
        topBar.setFixedHeight(88)

        topLayout = QVBoxLayout(topBar)
        topLayout.setContentsMargins(9, 6, 9, 6)
        topLayout.setSpacing(6)

        actionLayout = QHBoxLayout()
        actionLayout.setSpacing(8)

        self.search = SearchLineEdit()
        self.search.setPlaceholderText("搜索")

        self.search.textChanged.connect(self.applyFilter)

        addBtn = ToolButton(FIF.ADD)
        addBtn.clicked.connect(self.addTalk)

        imageBtn = ToolButton(FIF.PHOTO)
        imageBtn.setFixedSize(32, 32)
        imageBtn.setToolTip("添加图片")
        imageBtn.clicked.connect(self.addImageTalk)

        pdfBtn = ToolButton(FIF.DOCUMENT)
        pdfBtn.setFixedSize(32, 32)
        pdfBtn.setToolTip("添加 PDF")
        pdfBtn.clicked.connect(self.addPdfTalk)

        self.pinBtn = ToolButton(FIF.PIN)
        self.pinBtn.setCheckable(True)
        self.pinBtn.clicked.connect(self.toggleTopMost)
        self.setupWindowPinButton()

        actionLayout.addWidget(self.search)
        actionLayout.addWidget(addBtn)
        actionLayout.addWidget(imageBtn)
        actionLayout.addWidget(pdfBtn)
        actionLayout.addWidget(self.pinBtn)

        topLayout.addLayout(actionLayout)
        self.categoryBar = CategoryButtonBar(
            (ALL_CATEGORIES, *FIXED_CATEGORIES),
            self.selectedCategory,
            self.selectCategory,
            self
        )

        topLayout.addWidget(self.categoryBar)

        root.addWidget(topBar)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        scroll.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        scroll.setStyleSheet("""
            QScrollArea {
                background: transparent;
                border: none;
            }

            QScrollArea > QWidget > QWidget {
                background: transparent;
            }
        """)

        container = QWidget()

        self.listLayout = QVBoxLayout(container)
        self.listLayout.setContentsMargins(0, 0, 0, 0)
        self.listLayout.setSpacing(8)

        scroll.setWidget(container)

        root.addWidget(scroll)

        self.sortBtn = PushButton("调整顺序")
        self.sortBtn.setFixedHeight(30)
        self.sortBtn.setToolTip("拖动调整全部话术的顺序")
        self.sortBtn.clicked.connect(self.sortTalks)
        root.addWidget(self.sortBtn)

        self.refreshRows()

    def showTip(self, title, text):

        InfoBar.success(
            title=title,
            content=text,
            orient=Qt.Horizontal,
            isClosable=False,
            position=InfoBarPosition.BOTTOM,
            duration=1500,
            parent=self
        )

    def showWarning(self, title, text):

        InfoBar.warning(
            title=title,
            content=text,
            orient=Qt.Horizontal,
            isClosable=False,
            position=InfoBarPosition.BOTTOM,
            duration=1800,
            parent=self
        )

    def setupWindowPinButton(self):

        self.pinBtn.setObjectName("windowPinButton")
        self.pinBtn.setFixedSize(32, 32)
        self.pinBtn.setStyleSheet("""
            #windowPinButton {
                color: #64748b;
                background: #ffffff;
                border: 1px solid #d7e2ef;
                border-radius: 8px;
            }

            #windowPinButton:hover {
                color: #2563eb;
                background: #f5f9ff;
                border: 1px solid #9ebff3;
            }

            #windowPinButton:checked,
            #windowPinButton[topMost="true"] {
                color: #ffffff;
                background: #2563eb;
                border: 1px solid #1d4ed8;
            }

            #windowPinButton:checked:hover,
            #windowPinButton[topMost="true"]:hover {
                background: #1d4ed8;
                border: 1px solid #1e40af;
            }
        """)
        self.updateWindowPinButton()

    def selectCategory(self, category):

        self.selectedCategory = category
        self.applyFilter()

    def updateWindowPinButton(self):

        checked = self.pinBtn.isChecked()

        self.pinBtn.setToolTip("取消固定窗口" if checked else "固定窗口")
        self.pinBtn.setProperty("topMost", checked)
        self.pinBtn.style().unpolish(self.pinBtn)
        self.pinBtn.style().polish(self.pinBtn)
        self.pinBtn.update()

    def toggleTopMost(self):

        checked = self.pinBtn.isChecked()

        self.mainWindow.setTopMost(checked)
        self.updateWindowPinButton()

        if checked:
            self.showTip("窗口已置顶", "当前窗口将保持在最前面")
        else:
            self.showTip("已取消置顶", "窗口恢复普通模式")

    def loadData(self):

        DATA_FILE.parent.mkdir(parents=True, exist_ok=True)

        if not DATA_FILE.exists() and LEGACY_DATA_FILE.exists():

            with open(LEGACY_DATA_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)

            with open(DATA_FILE, "w", encoding="utf-8") as f:
                json.dump(data, f, ensure_ascii=False, indent=4)

            return data

        if not DATA_FILE.exists():

            default = [
                {"content": "您好，这边可以帮您办理宽带套餐", "category": "开场"},
                {"content": "现在办理可享受限时优惠活动", "category": "优惠"},
                {"content": "请问您的安装地址在哪里", "category": "信息确认"}
            ]

            with open(DATA_FILE, "w", encoding="utf-8") as f:
                json.dump(default, f, ensure_ascii=False, indent=4)

        with open(DATA_FILE, "r", encoding="utf-8") as f:
            return json.load(f)

    def saveData(self):

        DATA_FILE.parent.mkdir(parents=True, exist_ok=True)

        temporaryFile = DATA_FILE.with_name(f".{DATA_FILE.name}.{uuid.uuid4().hex}.tmp")

        try:
            with open(temporaryFile, "w", encoding="utf-8") as f:
                json.dump(self.data, f, ensure_ascii=False, indent=4)

            temporaryFile.replace(DATA_FILE)
        finally:
            try:
                temporaryFile.unlink(missing_ok=True)
            except OSError:
                pass

    def isImageTalk(self, talk):

        return talk.get("type") == "image"

    def isPdfTalk(self, talk):

        return talk.get("type") == "pdf"

    def pdfFilePath(self, talk):

        path = Path(talk.get("pdf", ""))

        if path.is_absolute():
            return path

        return APP_DATA_DIR / path

    def imageFilePath(self, talk):

        image = talk.get("image", "")
        path = Path(image)

        if path.is_absolute():
            return path

        return APP_DATA_DIR / path

    def talkTooltip(self, talk):

        if self.isPdfTalk(talk):
            return str(self.pdfFilePath(talk))

        if self.isImageTalk(talk):
            return str(self.imageFilePath(talk))

        return talk.get("content", "")

    def talkCategory(self, talk):

        category = talk.get("category", "").strip()

        if category in FIXED_CATEGORIES:
            return category

        return ""

    def setTalkCategoryValue(self, talk, category):

        category = category.strip()

        if category in FIXED_CATEGORIES:
            talk["category"] = category
        else:
            talk.pop("category", None)

    def currentSelectedCategory(self):

        if self.selectedCategory == ALL_CATEGORIES:
            return ""

        return self.selectedCategory

    def talkSearchText(self, talk):

        if self.isPdfTalk(talk):
            return f"PDF {self.pdfFilePath(talk).name} {self.talkCategory(talk)}"

        if self.isImageTalk(talk):
            return f"图片 {self.imageFilePath(talk).name} {self.talkCategory(talk)}"

        return f"{talk.get('content', '')} {self.talkCategory(talk)}"

    def copyImageToLibrary(self, sourcePath):

        source = Path(sourcePath)

        if source.suffix.lower() not in IMAGE_EXTENSIONS:
            self.showWarning("格式不支持", "请选择常见图片格式")
            return None

        IMAGE_DIR.mkdir(parents=True, exist_ok=True)

        targetName = f"{uuid.uuid4().hex}{source.suffix.lower()}"
        target = IMAGE_DIR / targetName

        shutil.copy2(source, target)

        return Path("images") / targetName

    def selectImageFile(self):

        filePath, _ = QFileDialog.getOpenFileName(
            self,
            "选择图片",
            "",
            "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"
        )

        return filePath

    def selectPdfFile(self):

        filePath, _ = QFileDialog.getOpenFileName(
            self,
            "选择 PDF",
            "",
            "PDF 文件 (*.pdf *.PDF)"
        )

        return filePath

    def copyPdfToLibrary(self, sourcePath):

        source = Path(sourcePath)

        if source.suffix.lower() != ".pdf":
            self.showWarning("格式不支持", "请选择 PDF 文件")
            return None

        # 每份 PDF 使用独立目录，保留发送时的原文件名，也避免同名文件覆盖。
        relativePath = Path("pdfs") / uuid.uuid4().hex / source.name
        target = APP_DATA_DIR / relativePath

        try:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
        except OSError as error:
            self.removePdfFile(target)
            self.showWarning("导入失败", f"无法读取或保存 PDF：{error}")
            return None

        return relativePath

    def addPdfTalk(self):

        filePath = self.selectPdfFile()

        if not filePath:
            return

        pdfPath = self.copyPdfToLibrary(filePath)

        if not pdfPath:
            return

        talk = {"type": "pdf", "pdf": pdfPath.as_posix()}
        self.setTalkCategoryValue(talk, self.currentSelectedCategory())

        if not self.saveNewTalk(talk):
            self.removePdfFile(self.pdfFilePath(talk))
            return

        self.showTip("新增成功", "PDF 已添加")

    def replacePdfTalk(self, talk):

        filePath = self.selectPdfFile()

        if not filePath:
            return

        pdfPath = self.copyPdfToLibrary(filePath)

        if not pdfPath:
            return

        oldPdf = talk.get("pdf", "")
        oldPdfPath = self.pdfFilePath(talk)
        talk["pdf"] = pdfPath.as_posix()

        try:
            self.saveData()
        except OSError as error:
            self.removePdfFile(self.pdfFilePath(talk))
            talk["pdf"] = oldPdf
            self.showWarning("保存失败", f"PDF 未替换，请重试：{error}")
            return

        self.removePdfFile(oldPdfPath)
        self.refreshRows()
        self.showTip("替换成功", "PDF 已更新")

    def removePdfFile(self, pdfPath):

        try:
            pdfPath = Path(pdfPath).resolve()

            # 只清理本软件 PDF 库中独立目录内的副本。
            if pdfPath.parent.parent != PDF_DIR.resolve():
                return

            pdfPath.unlink(missing_ok=True)
            pdfPath.parent.rmdir()
        except OSError:
            pass

    def addTalk(self):

        dialog = EditDialog(
            self,
            category=self.currentSelectedCategory(),
            categories=CATEGORY_SELECT_OPTIONS
        )

        if dialog.exec():

            data = dialog.getData()
            text = data["content"]

            if not text.strip():
                QMessageBox.warning(self, "提示", "内容不能为空")
                return

            talk = {"content": text}
            self.setTalkCategoryValue(talk, data["category"])

            if self.saveNewTalk(talk):
                self.showTip("新增成功", "话术已添加")

    def addImageTalk(self):

        filePath = self.selectImageFile()

        if not filePath:
            return

        imagePath = self.copyImageToLibrary(filePath)

        if not imagePath:
            return

        talk = {
            "type": "image",
            "image": imagePath.as_posix()
        }
        self.setTalkCategoryValue(talk, self.currentSelectedCategory())

        if not self.saveNewTalk(talk):
            self.removeImageFile(self.imageFilePath(talk))
            return

        self.showTip("新增成功", "图片已添加")

    def saveNewTalk(self, talk):

        index = self.pinnedTalkCount()
        self.data.insert(index, talk)

        try:
            self.saveData()
        except OSError as error:
            self.data.pop(index)
            self.showWarning("新增失败", f"无法保存话术，请重试：{error}")
            return False

        self.refreshRows()
        return True

    def replaceImageTalk(self, talk):

        filePath = self.selectImageFile()

        if not filePath:
            return

        oldImagePath = self.imageFilePath(talk)
        imagePath = self.copyImageToLibrary(filePath)

        if not imagePath:
            return

        talk["type"] = "image"
        talk["image"] = imagePath.as_posix()

        self.removeImageFile(oldImagePath)

        self.saveData()
        self.refreshRows()

        self.showTip("替换成功", "图片已更新")

    def removeImageFile(self, imagePath):

        try:
            imagePath = Path(imagePath)

            if imagePath.exists() and imagePath.parent == IMAGE_DIR:
                imagePath.unlink()
        except:
            pass

    def findTalkIndex(self, target):

        for index, item in enumerate(self.data):
            if item is target:
                return index

        return -1

    def setTalkCategory(self, talk):

        dialog = CategoryDialog(
            self,
            self.talkCategory(talk)
        )

        if not dialog.exec():
            return

        self.setTalkCategoryValue(talk, dialog.getCategory())
        self.saveData()
        self.refreshRows()

        self.showTip("分类已更新", "话术分类已保存")

    def isTalkPinned(self, talk):

        return talk.get("pinned", False)

    def pinnedTalkCount(self):

        return sum(1 for item in self.data if self.isTalkPinned(item))

    def pinTalk(self, talk):

        if self.setTalkPinned(talk, True):
            self.showTip("置顶成功", "话术已移动到顶部")

    def unpinTalk(self, talk):

        if self.setTalkPinned(talk, False):
            self.showTip("已取消置顶", "话术已移到普通列表最前")

    def setTalkPinned(self, talk, pinned):

        if self.findTalkIndex(talk) < 0:
            return False

        oldTalk = dict(talk)
        talk["pinned"] = pinned
        talk.pop("original_index", None)
        ordered = [item for item in self.data if item is not talk]
        targetIndex = 0 if pinned else sum(1 for item in ordered if self.isTalkPinned(item))
        ordered.insert(targetIndex, talk)

        if not self.saveTalkOrder(ordered):
            talk.clear()
            talk.update(oldTalk)
            return False

        return True

    def sortTalks(self):

        dialog = SortDialog(self)

        if dialog.exec() and self.saveTalkOrder(dialog.orderedTalks()):
            self.showTip("排序已保存", "下次打开仍会保留这个顺序")

    def saveTalkOrder(self, ordered):

        previous = self.data
        self.data = ordered

        try:
            self.saveData()
        except OSError as error:
            self.data = previous
            self.showWarning("排序保存失败", f"无法保存顺序，请重试：{error}")
            return False

        self.refreshRows()
        return True

    def deleteTalk(self, talk):

        index = self.findTalkIndex(talk)

        if index < 0:
            return

        result = QMessageBox.question(
            self,
            "删除话术",
            "确定要删除这条话术吗？",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No
        )

        if result != QMessageBox.StandardButton.Yes:
            return

        talk = self.data.pop(index)

        try:
            self.saveData()
        except OSError as error:
            self.data.insert(index, talk)
            self.showWarning("删除失败", f"无法保存删除结果，请重试：{error}")
            return

        if self.isImageTalk(talk):
            self.removeImageFile(self.imageFilePath(talk))
        elif self.isPdfTalk(talk):
            self.removePdfFile(self.pdfFilePath(talk))

        self.refreshRows()

        self.showTip("删除成功", "话术已删除")

    def refreshRows(self):

        while self.listLayout.count():

            item = self.listLayout.takeAt(0)

            widget = item.widget()

            if widget:
                widget.deleteLater()

        self.rows = []

        for item in self.data:

            row = TalkRow(item, self)

            self.rows.append((item, row))
            self.listLayout.addWidget(row)

        self.listLayout.addStretch()
        self.sortBtn.setEnabled(len(self.data) > 1)
        self.applyFilter()

    def applyFilter(self):

        keyword = self.search.text().lower()
        category = self.selectedCategory

        for item, row in self.rows:
            searchMatched = keyword in self.talkSearchText(item).lower()
            itemCategory = self.talkCategory(item)

            if category == ALL_CATEGORIES:
                categoryMatched = True
            else:
                categoryMatched = itemCategory == category

            row.setVisible(searchMatched and categoryMatched)


class MainWindow(QWidget):

    def __init__(self):
        super().__init__()

        self.resize(300, 600)

        self.setMinimumSize(300, 700)
        self.setMaximumWidth(300)

        self.setWindowIcon(self.createAppIcon())
        self.setWindowTitle("鑫牛金牌销售话术库")
        self.setObjectName("mainWindow")
        self.setStyleSheet("""
            #mainWindow {
                background: #eef4fa;
            }
        """)
        self.setWindowFlag(Qt.WindowMaximizeButtonHint, False)

        page = HomePage(self)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        layout.addWidget(page)

        self.dragOverlay = DragOverlay(self)
        self.dragOverlay.hide()

        self.moveToRight()

    def createAppIcon(self):

        iconPath = resourcePath(APP_ICON_FILE)

        if iconPath.exists():
            return QIcon(str(iconPath))

        return QIcon()

    def setTopMost(self, checked):

        try:

            hwnd = int(self.winId())

            HWND_TOPMOST = -1
            HWND_NOTOPMOST = -2
            SWP_NOSIZE = 0x0001
            SWP_NOMOVE = 0x0002
            SWP_NOACTIVATE = 0x0010

            insertAfter = HWND_TOPMOST if checked else HWND_NOTOPMOST
            flags = SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE

            result = windll.user32.SetWindowPos(
                hwnd,
                insertAfter,
                0,
                0,
                0,
                0,
                flags
            )

            if not result:
                raise OSError("SetWindowPos failed")

        except:

            self.setWindowFlag(Qt.WindowStaysOnTopHint, checked)
            self.show()

    def showDragOverlay(self):

        self.dragOverlay.setGeometry(self.rect())
        self.dragOverlay.raise_()
        self.dragOverlay.show()
        QApplication.processEvents()

    def hideDragOverlay(self):

        self.dragOverlay.hide()

    def resizeEvent(self, event):

        super().resizeEvent(event)

        if hasattr(self, "dragOverlay"):
            self.dragOverlay.setGeometry(self.rect())

    def moveToRight(self):

        screen = QGuiApplication.primaryScreen().availableGeometry()

        geo = self.geometry()

        x = screen.width() - geo.width() - 8
        y = (screen.height() - geo.height()) // 2

        self.move(x, y)


if __name__ == "__main__":

    app = QApplication(sys.argv)

    setTheme(Theme.LIGHT)

    window = MainWindow()

    window.show()

    sys.exit(app.exec())


