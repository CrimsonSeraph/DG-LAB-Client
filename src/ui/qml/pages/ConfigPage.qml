import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 配置页：连接（IP/端口/二维码）、波形卡、主题卡、日志卡，以及规则编辑器入口。
Item {
    id: page

    objectName: "configPage"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin
        spacing: Metrics.cardSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            Text {
                text: qsTr("配置")
                color: Theme.textPrimary
                font.pixelSize: Typography.fontTitle
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "configRuleEditorButton"
                text: qsTr("规则编辑器")
            }
        }

        GlassCard {
            Layout.fillWidth: true
            title: qsTr("连接")

            Text {
                width: parent.width
                text: qsTr("连接卡片将在阶段 3 实现。")
                color: Theme.textMuted
                font.pixelSize: Typography.fontBody
                wrapMode: Text.WordWrap
            }
        }

        GlassCard {
            Layout.fillWidth: true
            title: qsTr("波形")

            Text {
                width: parent.width
                text: qsTr("波形卡片（选择波形 / 创建波形）将在阶段 3、6 实现。")
                color: Theme.textMuted
                font.pixelSize: Typography.fontBody
                wrapMode: Text.WordWrap
            }
        }

        GlassCard {
            Layout.fillWidth: true
            title: qsTr("主题")

            Text {
                width: parent.width
                text: qsTr("主题卡片（当前主题 / 主色 / 副色、选择与自定义）将在阶段 3、5 实现。")
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
