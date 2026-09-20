import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

# Keep the tests away from the user's desktop and system clipboard.
os.environ["QT_QPA_PLATFORM"] = "offscreen"
if Path("C:/Windows/Fonts").is_dir():
    os.environ.setdefault("QT_QPA_FONTDIR", "C:/Windows/Fonts")

import main
from PySide6.QtCore import QCoreApplication, QEvent, Qt
from PySide6.QtGui import QColor, QFontDatabase, QPainter, QPdfWriter, QPixmap


class TalkboxTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = main.QApplication.instance() or main.QApplication([])
        font = Path("C:/Windows/Fonts/msyh.ttc")
        if font.is_file():
            QFontDatabase.addApplicationFont(str(font))

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="talkbox-pdf-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.library = self.root / "Talkbox"
        self.library.mkdir()
        self.sources = self.root / "sources"
        self.sources.mkdir()
        paths = patch.multiple(
            main,
            APP_DATA_DIR=self.library,
            DATA_FILE=self.library / "talk_data.json",
            IMAGE_DIR=self.library / "images",
            PDF_DIR=self.library / "pdfs",
            LEGACY_DATA_FILE=self.root / "legacy.json",
        )
        paths.start()
        self.addCleanup(paths.stop)
        main.DATA_FILE.write_text("[]", encoding="utf-8")
        self.window = main.MainWindow()
        self.page = self.window.findChild(main.HomePage)
        self.page.showTip = Mock()
        self.page.showWarning = Mock()
        self.window.show()
        self.app.processEvents()

    def tearDown(self):
        self.app.clipboard().clear()
        self.app.processEvents()
        self.window.close()
        self.window.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
        self.app.processEvents()

    def makePdf(self, name="产品报价 2026.PDF", directory=None):
        path = (directory or self.sources) / name
        path.parent.mkdir(parents=True, exist_ok=True)
        writer = QPdfWriter(str(path))
        painter = QPainter(writer)
        self.assertTrue(painter.isActive())
        painter.drawText(100, 100, "TalkBOX PDF import fixture")
        painter.end()
        del writer
        self.assertTrue(path.read_bytes().startswith(b"%PDF-"))
        return path

    def importPdf(self, path):
        existing = {id(talk) for talk in self.page.data}
        with patch.object(main.QFileDialog, "getOpenFileName", return_value=(str(path), "")):
            self.page.addPdfTalk()
        return next(talk for talk in self.page.data if id(talk) not in existing)

    def pdfFiles(self):
        return sorted(path for path in main.PDF_DIR.rglob("*") if path.is_file())

    def savedData(self):
        return json.loads(main.DATA_FILE.read_text(encoding="utf-8"))


class PdfImportTests(TalkboxTestCase):
    def test_toolbar_import_keeps_filename_bytes_category_and_survives_reload(self):
        source = self.makePdf()
        original = source.read_bytes()
        self.page.categoryBar.buttons["报价"].click()
        button = next(
            button for button in self.page.findChildren(main.ToolButton)
            if button.toolTip() == "添加 PDF"
        )
        with patch.object(main.QFileDialog, "getOpenFileName", return_value=(str(source), "")):
            button.click()
        self.assertEqual(len(self.page.data), 1)
        talk = self.page.data[0]
        stored = self.page.pdfFilePath(talk)
        self.assertEqual(talk["category"], "报价")
        self.assertFalse(Path(talk["pdf"]).is_absolute())
        self.assertEqual(stored.name, source.name)
        self.assertEqual(stored.read_bytes(), original)
        self.assertEqual(source.read_bytes(), original)
        self.assertEqual(self.page.rows[0][1].fullText, source.name)
        self.assertEqual(self.page.rows[0][1].textLabel.toolTip(), str(stored))
        self.assertEqual(self.savedData(), self.page.data)
        source.unlink()
        reloaded = main.HomePage(self.window)
        self.assertEqual(reloaded.data, self.page.data)
        self.assertEqual(reloaded.pdfFilePath(reloaded.data[0]).read_bytes(), original)
        self.assertTrue(reloaded.rows[0][1].isPdf)
        reloaded.deleteLater()

    def test_same_named_pdfs_are_independent_copies(self):
        first = self.makePdf(directory=self.sources / "first")
        second = self.makePdf(directory=self.sources / "second")
        firstTalk = self.importPdf(first)
        secondTalk = self.importPdf(second)
        firstStored = self.page.pdfFilePath(firstTalk)
        secondStored = self.page.pdfFilePath(secondTalk)
        self.assertNotEqual(firstStored, secondStored)
        self.assertEqual(firstStored.name, secondStored.name)
        self.assertEqual(firstStored.read_bytes(), first.read_bytes())
        self.assertEqual(secondStored.read_bytes(), second.read_bytes())
        self.assertEqual(len(self.pdfFiles()), 2)

    def test_cancel_unsupported_missing_and_directory_sources_do_not_import(self):
        directory = self.sources / "folder.pdf"
        directory.mkdir()
        for path in ("", self.sources / "note.txt", self.sources / "missing.pdf", directory):
            with self.subTest(path=str(path)):
                with patch.object(main.QFileDialog, "getOpenFileName", return_value=(str(path), "")):
                    self.page.addPdfTalk()
                self.assertEqual(self.page.data, [])
                self.assertEqual(self.savedData(), [])
                self.assertEqual(self.pdfFiles(), [])

    def test_copy_failure_cleans_partial_file(self):
        source = self.makePdf()

        def failCopy(sourcePath, target):
            Path(target).write_bytes(b"partial copy")
            raise OSError("Disk full")

        with patch.object(main.shutil, "copy2", side_effect=failCopy):
            self.assertIsNone(self.page.copyPdfToLibrary(source))
        self.assertTrue(source.is_file())
        self.assertEqual(list(main.PDF_DIR.iterdir()), [])
        self.assertEqual(self.savedData(), [])
        self.page.showWarning.assert_called_once()

    def test_failed_save_preserves_existing_json_and_rolls_back_import(self):
        source = self.makePdf()
        existing = {"content": "原有话术", "category": "开场"}
        self.page.data.append(existing)
        self.page.saveData()
        originalJson = main.DATA_FILE.read_bytes()

        def failWrite(data, output, **kwargs):
            output.write("[")
            raise OSError("Disk full")

        with patch.object(main.json, "dump", side_effect=failWrite):
            with patch.object(self.page, "selectPdfFile", return_value=str(source)):
                self.page.addPdfTalk()
        self.assertEqual(self.page.data, [existing])
        self.assertEqual(main.DATA_FILE.read_bytes(), originalJson)
        self.assertEqual(self.pdfFiles(), [])
        self.assertEqual(list(self.library.glob("*.tmp")), [])
        self.page.showWarning.assert_called_once()

    def test_copy_and_double_click_put_pdf_file_on_clipboard(self):
        talk = self.importPdf(self.makePdf())
        stored = self.page.pdfFilePath(talk)
        row = self.page.rows[0][1]
        for copy in (row.copyTalk, lambda: row.doubleCopy(None)):
            with self.subTest(copy=copy):
                self.app.clipboard().setText("previous clipboard")
                copy()
                mime = self.app.clipboard().mimeData()
                self.assertTrue(mime.hasUrls())
                self.assertEqual([Path(url.toLocalFile()) for url in mime.urls()], [stored])
                self.assertTrue(Path(mime.urls()[0].toLocalFile()).is_file())

    def test_drag_transfers_pdf_file_and_removes_overlay(self):
        talk = self.importPdf(self.makePdf())
        with patch.object(main, "QDrag") as drag:
            self.page.rows[0][1].startTextDrag()
        mime = drag.return_value.setMimeData.call_args.args[0]
        self.assertEqual(Path(mime.urls()[0].toLocalFile()), self.page.pdfFilePath(talk))
        drag.return_value.exec.assert_called_once_with(Qt.DropAction.CopyAction)
        self.assertTrue(self.window.dragOverlay.isHidden())

    def test_missing_stored_pdf_warns_without_changing_clipboard_or_starting_drag(self):
        talk = self.importPdf(self.makePdf())
        self.page.pdfFilePath(talk).unlink()
        self.app.clipboard().setText("keep clipboard")
        row = self.page.rows[0][1]
        with patch.object(main, "QDrag") as drag:
            row.copyTalk()
            row.startTextDrag()
            drag.assert_not_called()
        self.assertEqual(self.app.clipboard().text(), "keep clipboard")
        self.assertEqual(self.page.showWarning.call_count, 2)

    def test_edit_replaces_pdf_preserves_metadata_and_keeps_original_sources(self):
        source = self.makePdf()
        talk = self.importPdf(source)
        self.page.setTalkCategoryValue(talk, "报价")
        self.page.pinTalk(talk)
        oldStored = self.page.pdfFilePath(talk)
        metadata = {key: value for key, value in talk.items() if key != "pdf"}
        replacement = self.makePdf("新版产品报价.pdf")
        with patch.object(self.page, "selectPdfFile", return_value=str(replacement)):
            self.page.rows[0][1].editTalk()
        self.assertEqual({key: value for key, value in talk.items() if key != "pdf"}, metadata)
        self.assertEqual(self.page.pdfFilePath(talk).read_bytes(), replacement.read_bytes())
        self.assertFalse(oldStored.parent.exists())
        self.assertTrue(source.is_file())
        self.assertTrue(replacement.is_file())
        self.assertEqual(self.savedData(), self.page.data)

    def test_replacement_cancel_copy_failure_and_save_failure_keep_old_pdf(self):
        talk = self.importPdf(self.makePdf())
        oldTalk = dict(talk)
        oldStored = self.page.pdfFilePath(talk)
        originalJson = main.DATA_FILE.read_bytes()
        replacement = self.makePdf("replacement.pdf")
        with patch.object(self.page, "selectPdfFile", return_value=""):
            self.page.replacePdfTalk(talk)
        with patch.object(self.page, "selectPdfFile", return_value=str(replacement)):
            with patch.object(main.shutil, "copy2", side_effect=PermissionError("Cannot read")):
                self.page.replacePdfTalk(talk)
            with patch.object(Path, "replace", side_effect=PermissionError("Cannot save")):
                self.page.replacePdfTalk(talk)
        self.assertEqual(talk, oldTalk)
        self.assertEqual(self.pdfFiles(), [oldStored])
        self.assertEqual(main.DATA_FILE.read_bytes(), originalJson)
        self.assertEqual(list(self.library.glob("*.tmp")), [])

    def test_delete_requires_confirmation_and_preserves_original_file(self):
        source = self.makePdf()
        talk = self.importPdf(source)
        stored = self.page.pdfFilePath(talk)
        with patch.object(main.QMessageBox, "question", return_value=main.QMessageBox.StandardButton.No):
            self.page.deleteTalk(talk)
        self.assertTrue(stored.is_file())
        self.assertEqual(self.page.data, [talk])
        with patch.object(main.QMessageBox, "question", return_value=main.QMessageBox.StandardButton.Yes):
            self.page.deleteTalk(talk)
        self.assertEqual(self.page.data, [])
        self.assertEqual(self.savedData(), [])
        self.assertFalse(stored.parent.exists())
        self.assertTrue(source.is_file())

    def test_failed_delete_keeps_record_and_pdf(self):
        talk = self.importPdf(self.makePdf())
        stored = self.page.pdfFilePath(talk)
        with patch.object(main.QMessageBox, "question", return_value=main.QMessageBox.StandardButton.Yes):
            with patch.object(Path, "replace", side_effect=PermissionError("Cannot save")):
                self.page.deleteTalk(talk)
        self.assertEqual(self.page.data, [talk])
        self.assertEqual(self.savedData(), [talk])
        self.assertTrue(stored.is_file())
        self.page.showWarning.assert_called_once()

    def test_cleanup_never_deletes_files_outside_managed_pdf_subfolders(self):
        source = self.makePdf()
        rootFile = self.makePdf("keep.pdf", main.PDF_DIR)
        for path in (source, rootFile, main.PDF_DIR / ".." / ".." / "sources" / source.name):
            with self.subTest(path=path):
                self.page.removePdfFile(path)
                self.assertTrue(source.is_file())
                self.assertTrue(rootFile.is_file())

    def test_pdf_search_categories_and_existing_text_image_copy(self):
        text = {"content": "您好，欢迎咨询", "category": "开场"}
        self.page.data.append(text)
        sourceImage = self.sources / "sample.png"
        pixmap = QPixmap(16, 16)
        pixmap.fill(QColor("#2563eb"))
        self.assertTrue(pixmap.save(str(sourceImage)))
        imagePath = self.page.copyImageToLibrary(sourceImage)
        image = {"type": "image", "image": imagePath.as_posix(), "category": "报价"}
        self.page.data.append(image)
        self.page.selectCategory("报价")
        pdf = self.importPdf(self.makePdf())

        def visible():
            return [talk for talk, row in self.page.rows if not row.isHidden()]

        self.assertEqual(visible(), [pdf, image])
        self.page.search.setText("产品报价")
        self.assertEqual(visible(), [pdf])
        self.page.search.setText("pDf")
        self.assertEqual(visible(), [pdf])
        self.page.selectCategory("开场")
        self.assertEqual(visible(), [])
        self.page.search.clear()
        self.assertEqual(visible(), [text])
        self.page.selectCategory(main.ALL_CATEGORIES)
        self.assertEqual(visible(), [pdf, text, image])
        with patch.object(main.InfoBar, "success"):
            textRow = next(row for talk, row in self.page.rows if talk is text)
            textRow.copyTalk()
            self.assertEqual(self.app.clipboard().text(), text["content"])
            self.assertEqual(textRow.createDragMimeData().text(), text["content"])
            imageRow = next(row for talk, row in self.page.rows if talk is image)
            imageRow.copyTalk()
            self.assertFalse(self.app.clipboard().pixmap().isNull())
            self.assertTrue(imageRow.createDragMimeData().hasImage())


if __name__ == "__main__":
    unittest.main()
