import QtQuick
import QtQuick.Controls
import TalkBox

// Shown when the data directories or the database cannot be initialized.
// The main window is never opened in a broken state.
ApplicationWindow {
    id: errorWindow

    width: 560
    height: 320
    visible: true
    title: qsTr("TalkBOX 启动失败")
    color: Theme.pageBackground

    Column {
        anchors.centerIn: parent
        width: parent.width - 96
        spacing: 12

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("无法初始化本地数据")
            color: Theme.danger
            font.pixelSize: 17
            font.weight: Font.DemiBold
        }

        Text {
            width: parent.width
            text: startupErrorMessage
            color: Theme.textPrimary
            font.pixelSize: 13
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            width: parent.width
            text: qsTr("数据目录：%1").arg(AppInfo.dataDirectory)
            color: Theme.textSecondary
            font.pixelSize: 12
            wrapMode: Text.WrapAnywhere
            horizontalAlignment: Text.AlignHCenter
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10

            AppButton {
                text: qsTr("打开数据目录")
                variant: "secondary"
                onClicked: AppInfo.openDataDirectory()
            }

            AppButton {
                text: qsTr("退出")
                onClicked: Qt.quit()
            }
        }
    }
}
