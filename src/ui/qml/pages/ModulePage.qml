import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 模块页：统一查询周期、插件/静态模块卡片、数值展示。
Item {
    id: page

    objectName: "modulePage"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin
        spacing: Metrics.cardSpacing

        Text {
            text: qsTr("模块")
            color: Theme.textPrimary
            font.pixelSize: Typography.fontTitle
            font.bold: true
        }

        GlassCard {
            Layout.fillWidth: true
            title: qsTr("数值模块")

            Text {
                width: parent.width
                text: qsTr("模块卡片与数值展示将在阶段 4 实现。")
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
