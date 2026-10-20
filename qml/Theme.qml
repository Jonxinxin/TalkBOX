pragma Singleton

import QtQuick

// Design tokens of TalkBOX (PRD 4.3). Every screen pulls its colors,
// radii and timing from here so the look stays consistent.
QtObject {
    readonly property color primary: "#2864E8"
    readonly property color primaryHover: "#1F58D5"
    readonly property color primaryPressed: "#1B4FC0"
    readonly property color primarySoft: "#EEF4FF"
    readonly property color pageBackground: "#F3F5F9"
    /// Custom title bar: a touch lighter than the page so the caption reads as
    /// its own band, separated by a hairline (no QML shadow).
    readonly property color titleBarBackground: "#F8FAFD"
    readonly property color cardBackground: "#FFFFFF"
    readonly property color border: "#DDE5F0"
    readonly property color borderStrong: "#B7CCFF"
    readonly property color focusBorder: "#8CB0FF"
    readonly property color textPrimary: "#203A5F"
    readonly property color textSecondary: "#71839A"
    readonly property color textMuted: "#8294AF"
    readonly property color textOnPrimary: "#FFFFFF"
    readonly property color danger: "#C04B57"
    readonly property color dangerSoft: "#FBEAEC"

    readonly property int radiusLarge: 12
    readonly property int radiusMedium: 10
    readonly property int radiusSmall: 8
    /// Height of the header controls (search field, new / pin / settings).
    readonly property int controlHeight: 34
    readonly property int animationFast: 140

    readonly property var chipPalette: ({
        blue: { background: "#EAF1FF", foreground: "#2F6FE0", dot: "#3B82F6" },
        teal: { background: "#E4F5EF", foreground: "#0E8A6D", dot: "#10B981" },
        amber: { background: "#FBF1DF", foreground: "#A8730A", dot: "#E0A82E" },
        rose: { background: "#FBEAEC", foreground: "#C04B57", dot: "#E46378" },
        slate: { background: "#EEF2F7", foreground: "#64748B", dot: "#94A3B8" },
        violet: { background: "#F0ECFB", foreground: "#7C5CD6", dot: "#8B5CF6" }
    })

    function chip(key) {
        return chipPalette[key] !== undefined ? chipPalette[key] : chipPalette.violet
    }

    function chipBackground(key) {
        return chip(key).background
    }

    function chipForeground(key) {
        return chip(key).foreground
    }

    function chipDot(key) {
        return chip(key).dot
    }
}
