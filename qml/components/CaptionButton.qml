import QtQuick
import TalkBox

// Window control button of the custom title bar (minimise / maximise / close).
//
// Geometry follows the Windows 11 / Edge caption buttons: a 46 DIP wide,
// caption-high hit area, a flat (unrounded) hover wash, and a red full-bleed
// close button whose glyph turns white.
//
// Deliberately *not* a Button: a control resizes its contentItem to the whole
// padded area, which made the glyph size unpredictable. As a plain Item the
// glyph box below is exact, and the caption button can never take focus (so it
// can never paint a focus ring - the old "black flash").
Item {
    id: control

    property url iconSource
    property url hoverIconSource
    property bool danger: false
    /// Glyph box, not hit area. Windows draws a ~10 DIP glyph in a 32 DIP
    /// caption; 14 keeps that ratio in this 42 DIP caption.
    property int iconSize: 14
    property bool hovered: hoverHandler.hovered
    property bool down: tapHandler.pressed

    implicitWidth: 46
    implicitHeight: 42

    Rectangle {
        anchors.fill: parent
        radius: 0
        color: control.danger
               ? (control.down ? "#B02A1B" : control.hovered ? "#C42B1C" : "transparent")
               : (control.down ? "#DCE3EC" : control.hovered ? "#E7ECF3" : "transparent")
    }

    Image {
        width: control.iconSize
        height: control.iconSize
        anchors.centerIn: parent
        smooth: true
        fillMode: Image.PreserveAspectFit
        // Decode above the display size so the glyph stays crisp at 125 % /
        // 150 % display scaling.
        sourceSize.width: Math.ceil(control.iconSize * 2)
        sourceSize.height: Math.ceil(control.iconSize * 2)
        source: control.hovered && control.hoverIconSource.toString() !== ""
                ? control.hoverIconSource
                : control.iconSource
    }

    HoverHandler {
        id: hoverHandler
        cursorShape: Qt.ArrowCursor
    }

    TapHandler {
        id: tapHandler
        acceptedButtons: Qt.LeftButton
        onTapped: control.clicked()
    }
    signal clicked()
}
