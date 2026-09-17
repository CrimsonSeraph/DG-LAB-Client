import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 关于页：应用信息、许可证与依赖说明。
Item {
    id: page

    objectName: "aboutPage"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin
        spacing: Metrics.cardSpacing

        Text {
            text: qsTr("关于")
            color: Theme.textPrimary
            font.pixelSize: Typography.fontTitle
            font.bold: true
        }

        GlassCard {
            Layout.fillWidth: true
            title: app.appName

            Text {
                width: parent.width
                text: qsTr("版本 %1").arg(app.appVersion)
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
            }

            Text {
                width: parent.width
                text: qsTr("一个基于 Qt 的 DG-Lab 桌面客户端：从外部数据源获取数值，经规则计算后控制设备输出。")
                color: Theme.textMuted
                font.pixelSize: Typography.fontBody
                wrapMode: Text.WordWrap
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
