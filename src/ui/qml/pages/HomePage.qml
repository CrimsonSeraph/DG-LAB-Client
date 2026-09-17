import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 首页：A/B 通道面板（强度 / 模块 / 规则 / 波形）与规则编辑器入口。
// 交互约定：本文件不写任何信号处理器，交互控件由 C++（UiConnector）按 objectName 连接。
Item {
    id: page

    objectName: "homePage"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin
        spacing: Metrics.cardSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            Text {
                text: qsTr("首页")
                color: Theme.textPrimary
                font.pixelSize: Typography.fontTitle
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "homeRuleEditorButton"
                text: qsTr("规则编辑器")
            }
        }

        GlassCard {
            Layout.fillWidth: true
            title: qsTr("通道面板")

            Text {
                width: parent.width
                text: qsTr("通道卡片（强度 / 模块 / 规则 / 波形）将在阶段 2 实现。")
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
