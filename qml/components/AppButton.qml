import QtQuick
import QtQuick.Controls
import TalkBox

// Custom flat button used by dialogs and empty states.
// Variants: "primary" | "secondary" | "danger".
Button {
    id: control

    property string variant: "primary"

    implicitHeight: 36
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: 13
    font.weight: variant === "primary" ? Font.Medium : Font.Normal

    background: Rectangle {
        radius: Theme.radiusMedium
        color: {
            if (!control.enabled)
                return "#F0F3F8"
            if (control.variant === "primary") {
                if (control.down)
                    return Theme.primaryPressed
                return control.hovered ? Theme.primaryHover : Theme.primary
            }
            if (control.variant === "danger")
                return control.hovered ? Theme.dangerSoft : Theme.cardBackground
            return control.hovered ? "#F9FBFF" : Theme.cardBackground
        }
        border.width: control.variant === "primary" ? 0 : 1
        border.color: control.variant === "danger" ? "#E8B9BE" : Theme.border

        Behavior on color {
            ColorAnimation { duration: Theme.animationFast }
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: {
            if (!control.enabled)
                return Theme.textMuted
            if (control.variant === "primary")
                return Theme.textOnPrimary
            if (control.variant === "danger")
                return Theme.danger
            return Theme.textSecondary
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
