import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TalkBox

// Category change dialog. Tapping a category applies immediately; Esc
// closes without any change (PRD 3.4 / 4.6).
Popup {
    id: dialog
    objectName: "changeCategoryDialog"

    property string snippetId: ""
    property string selectedCategoryId: ""

    signal changed()

    // Adaptive width: the app window can be a narrow side panel.
    width: Math.min(420, (Overlay.overlay ? Overlay.overlay.width : 480) - 28)
    modal: true
    focus: true
    // Esc or a click anywhere outside the panel dismisses it without change.
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

    function openFor(id, categoryId) {
        snippetId = id
        selectedCategoryId = categoryId
        errorLabel.visible = false
        open()
    }

    function applyTo(categoryId) {
        if (categoryId === dialog.selectedCategoryId) {
            close()
            return
        }
        if (SnippetService.changeCategory(dialog.snippetId, categoryId)) {
            close()
            changed()
        } else {
            errorLabel.text = SnippetService.lastError
            errorLabel.visible = true
        }
    }

    contentItem: ColumnLayout {
        spacing: 14

        Keys.onEscapePressed: dialog.close()

        Text {
            text: qsTr("修改分类")
            color: Theme.textPrimary
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        CategoryPicker {
            Layout.fillWidth: true
            selectedId: dialog.selectedCategoryId
            onPicked: function(id) { dialog.applyTo(id) }
        }

        Text {
            id: errorLabel
            Layout.fillWidth: true
            visible: false
            color: Theme.danger
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
    }
}
