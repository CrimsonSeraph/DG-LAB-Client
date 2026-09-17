import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 首页：A/B 通道面板（强度 / 模块 / 规则 / 波形）与规则编辑器、模块管理入口。
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
                objectName: "homeModuleEntryButton"
                text: qsTr("模块管理")
            }

            AppButton {
                objectName: "homeRuleEditorButton"
                text: qsTr("规则编辑器")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Metrics.cardSpacing

            ChannelPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: ComponentStyle.channelCardMinWidth
                channel: "A"
                strength: home.strengthA
                limit: home.limitA
                running: home.runningA
                modules: home.modulesA
                rules: home.rulesA
                waveName: home.waveA
            }

            ChannelPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: ComponentStyle.channelCardMinWidth
                channel: "B"
                strength: home.strengthB
                limit: home.limitB
                running: home.runningB
                modules: home.modulesB
                rules: home.rulesB
                waveName: home.waveB
            }
        }
    }
}
