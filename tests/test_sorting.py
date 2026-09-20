import copy
import json
from pathlib import Path
from unittest.mock import patch

from test_pdf_import import TalkboxTestCase
import main
from PySide6.QtCore import QModelIndex, QTimer
from PySide6.QtGui import QColor, QPixmap


class TalkSortingTests(TalkboxTestCase):
    def seed(self, talks):
        self.page.data = talks
        self.page.saveData()
        self.page.refreshRows()
        self.app.processEvents()

    def moveRow(self, listWidget, source, destination):
        self.assertTrue(listWidget.model().moveRow(QModelIndex(), source, QModelIndex(), destination))

    def finishSorting(self, dialog, save=True):
        text = "保存" if save else "取消"
        button = next(button for button in dialog.findChildren(main.QPushButton) if button.text() == text)
        with patch.object(main, "SortDialog", return_value=dialog):
            QTimer.singleShot(0, button.click)
            self.page.sortBtn.click()

    def test_all_new_talk_types_go_before_normal_talks_below_pins(self):
        pinned = [{"content": f"置顶 {index}", "pinned": True} for index in range(7)]
        old = [{"content": "原有话术一"}, {"content": "原有话术二"}]
        self.seed(pinned + old)
        with patch.object(main, "EditDialog") as dialog:
            dialog.return_value.exec.return_value = main.QDialog.DialogCode.Accepted
            dialog.return_value.getData.return_value = {"content": "新文字", "category": "报价"}
            self.page.addTalk()
        text = self.page.data[7]
        self.assertEqual(text["content"], "新文字")
        self.assertEqual(self.page.data, pinned + [text] + old)

        source = self.sources / "new.png"
        image = QPixmap(16, 16)
        image.fill(QColor("#2563eb"))
        self.assertTrue(image.save(str(source)))
        with patch.object(self.page, "selectImageFile", return_value=str(source)):
            self.page.addImageTalk()
        imageTalk = self.page.data[7]
        self.assertTrue(self.page.isImageTalk(imageTalk))
        pdf = self.importPdf(self.makePdf())
        self.assertEqual(self.page.data, pinned + [pdf, imageTalk, text] + old)
        self.assertEqual(self.savedData(), self.page.data)

    def test_more_than_five_pins_are_saved_and_loaded(self):
        talks = [{"content": f"话术 {index}"} for index in range(12)]
        self.seed(list(talks))
        for talk in talks:
            self.page.pinTalk(talk)
            self.app.processEvents()
        self.assertEqual(self.page.pinnedTalkCount(), 12)
        self.assertEqual(self.page.data, list(reversed(talks)))
        self.assertTrue(all(talk["pinned"] for talk in self.savedData()))
        self.page.showWarning.assert_not_called()
        reloaded = main.HomePage(self.window)
        self.assertEqual(reloaded.data, self.page.data)
        self.assertEqual(reloaded.pinnedTalkCount(), 12)
        reloaded.deleteLater()

    def test_drag_reorders_both_groups_and_saves_duplicate_records(self):
        pinned = [{"content": "置顶一", "pinned": True}, {"content": "置顶二", "pinned": True}]
        first = {"content": "重复话术"}
        second = {"content": "重复话术"}
        pdf = self.importPdf(self.makePdf())
        original = pinned + [first, second, pdf]
        self.seed(original)
        dialog = main.SortDialog(self.page)
        self.moveRow(dialog.pinnedList, 1, 0)
        self.moveRow(dialog.normalList, 2, 0)
        self.assertEqual(self.page.data, original)
        self.finishSorting(dialog)
        expected = [pinned[1], pinned[0], pdf, first, second]
        self.assertEqual([id(talk) for talk in self.page.data], [id(talk) for talk in expected])
        self.assertEqual(self.savedData(), expected)
        reloaded = main.HomePage(self.window)
        self.assertEqual(reloaded.data, expected)
        reloaded.deleteLater()

    def test_cancel_sorting_preserves_order_and_saved_data(self):
        original = [{"content": "第一条"}, {"content": "第二条"}, {"content": "第三条"}]
        self.seed(original)
        before = main.DATA_FILE.read_bytes()
        dialog = main.SortDialog(self.page)
        self.moveRow(dialog.normalList, 2, 0)
        self.finishSorting(dialog, save=False)
        self.assertIs(self.page.data, original)
        self.assertEqual(main.DATA_FILE.read_bytes(), before)

    def test_failed_sort_save_preserves_memory_rows_and_json(self):
        original = [{"content": "第一条"}, {"content": "第二条"}, {"content": "第三条"}]
        self.seed(original)
        before = main.DATA_FILE.read_bytes()
        dialog = main.SortDialog(self.page)
        self.moveRow(dialog.normalList, 2, 0)
        with patch.object(Path, "replace", side_effect=PermissionError("Cannot save")):
            self.finishSorting(dialog)
        self.assertIs(self.page.data, original)
        self.assertEqual([talk for talk, row in self.page.rows], original)
        self.assertEqual(main.DATA_FILE.read_bytes(), before)
        self.page.showWarning.assert_called_once()

    def test_sorting_includes_hidden_talks_and_keeps_active_filters(self):
        first = {"content": "报价一", "category": "报价"}
        hidden = {"content": "开场白", "category": "开场"}
        last = {"content": "报价二", "category": "报价"}
        self.seed([first, hidden, last])
        self.page.selectCategory("报价")
        self.page.search.setText("报价")
        dialog = main.SortDialog(self.page)
        self.assertEqual(dialog.normalList.count(), 3)
        self.moveRow(dialog.normalList, 2, 0)
        self.finishSorting(dialog)
        self.assertEqual(self.page.data, [last, first, hidden])
        self.assertEqual(self.page.search.text(), "报价")
        self.assertEqual(self.page.selectedCategory, "报价")
        self.assertEqual([talk for talk, row in self.page.rows if not row.isHidden()], [last, first])
        self.assertEqual(self.savedData(), [last, first, hidden])

    def test_unpin_moves_to_front_of_normal_group_and_ignores_legacy_index(self):
        first = {"content": "置顶一", "pinned": True, "original_index": 0}
        second = {"content": "置顶二", "pinned": True}
        normal = {"content": "普通话术"}
        self.seed([first, second, normal])
        self.page.unpinTalk(first)
        self.assertEqual(self.page.data, [second, first, normal])
        self.assertFalse(first["pinned"])
        self.assertNotIn("original_index", first)
        self.assertEqual(self.savedData(), self.page.data)

    def test_pin_and_unpin_save_failure_restore_flags_and_order(self):
        pinned = {"content": "已置顶", "pinned": True, "original_index": 4}
        normal = {"content": "普通话术"}
        original = [pinned, normal]
        self.seed(original)
        values = copy.deepcopy(original)
        for change in (lambda: self.page.pinTalk(normal), lambda: self.page.unpinTalk(pinned)):
            with self.subTest(change=change):
                with patch.object(Path, "replace", side_effect=PermissionError("Cannot save")):
                    change()
                self.assertIs(self.page.data, original)
                self.assertEqual(original, values)
                self.assertEqual(self.savedData(), values)

    def test_legacy_interleaved_pins_load_first_without_changing_group_order(self):
        normal = [{"content": "普通一"}, {"content": "普通二"}]
        pinned = [{"content": "置顶一", "pinned": True}, {"content": "置顶二", "pinned": True}]
        main.DATA_FILE.write_text(
            json.dumps([normal[0], pinned[0], normal[1], pinned[1]], ensure_ascii=False),
            encoding="utf-8",
        )
        reloaded = main.HomePage(self.window)
        self.assertEqual(reloaded.data, pinned + normal)
        reloaded.saveData()
        self.assertEqual(self.savedData(), pinned + normal)
        reloaded.deleteLater()
