import QtQuick
import QtQuick.Layouts

// 小游戏页骨架：玩法由后续任务填充。
Item {
    id: page

    objectName: "miniGamePage"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin
        spacing: Metrics.cardSpacing

        PageHeader {
            Layout.fillWidth: true
            title: qsTr("小游戏")
            subtitle: qsTr("娱乐玩法入口，独立于设备控制链路")
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Text {
                objectName: "miniGamePagePlaceholder"
                anchors.centerIn: parent
                text: qsTr("小游戏 · 敬请期待")
                color: Theme.textMuted
                font.pixelSize: Typography.fontSubtitle
            }
        }
    }
}
