import QtQuick
import QtQuick.Layouts

// 插件中心页骨架：插件列表与详情由后续任务填充。
Item {
    id: page

    objectName: "pluginCenterPage"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin
        spacing: Metrics.cardSpacing

        PageHeader {
            Layout.fillWidth: true
            title: qsTr("插件中心")
            subtitle: qsTr("已加载插件概览、启停与配置入口")
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Text {
                objectName: "pluginCenterPagePlaceholder"
                anchors.centerIn: parent
                text: qsTr("即将上线")
                color: Theme.textMuted
                font.pixelSize: Typography.fontSubtitle
            }
        }
    }
}
