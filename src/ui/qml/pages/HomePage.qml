import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 主页看板：设备状态与通道启停、通道模块/规则绑定、已启用插件、近期触发流水、A/B 波形预览。
// 强度增减与波形选择已移出本页（迷你窗 / 波形设置页负责），卡内只留只读数值与说明。
// 与左侧导航重复的「插件中心」「波形设置」页内入口已移除，只留尚未实现的迷你窗入口。
// 交互约定：页内导航按钮用 app.navigate，其余交互控件仍由 C++（UiConnector）按 objectName 连接。
Item {
    id: page

    objectName: "homePage"

    // 页面索引与 AppBridge::Page 一致（与 MainWindow 的页面常量同源）
    readonly property int pagePluginCenter: 4

    // 内容区宽度 = 窗口宽度 - 导航栏宽度，导航栏按窄态取值即可反推窗口临界宽度：
    // 内容区一旦窄于该临界宽度就降为单列，避免两列各占一半后被压到换行。
    readonly property bool singleColumn: page.width < Responsive.compactNavWidth - Metrics.navWidthCompact

    ScrollView {
        id: pageScroll

        objectName: "homeScrollView"
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        padding: Metrics.pageMargin
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: pageScroll.availableWidth
            spacing: Metrics.cardSpacing

            PageHeader {
                Layout.fillWidth: true
                title: qsTr("主页")
                subtitle: (device.outputReady ? qsTr("输出可用") : qsTr("输出不可用"))
                    + " · " + (ble.connected ? qsTr("蓝牙已连接") : qsTr("蓝牙未连接"))
                    + " · " + (home.channelAState === "idle" && home.channelBState === "idle"
                        ? qsTr("通道空闲") : qsTr("通道运行中"))

                AppButton {
                    objectName: "homePluginEntryButton"
                    text: qsTr("插件管理")
                }

                AppButton {
                    objectName: "homeRuleEditorButton"
                    text: qsTr("规则编辑器")
                }
            }

            // 迷你窗入口：尚未实现，按钮置灰并标注即将上线
            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingMd

                AppButton {
                    objectName: "homeMiniWindowEntryButton"
                    text: qsTr("迷你窗 · 即将上线")
                    enabled: false
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            // 两列看板：左列设备与通道，右列插件与流水；窄窗口降为单列
            GridLayout {
                objectName: "homeDashboardGrid"
                Layout.fillWidth: true
                columns: page.singleColumn ? 1 : 2
                columnSpacing: Metrics.cardSpacing
                rowSpacing: Metrics.cardSpacing

                ColumnLayout {
                    objectName: "homeDashboardLeftColumn"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.alignment: Qt.AlignTop
                    spacing: Metrics.cardSpacing

                    DashboardDeviceCard {
                        Layout.fillWidth: true
                    }

                    DashboardChannelCard {
                        Layout.fillWidth: true
                        channel: "A"
                        modules: home.modulesA
                        rules: home.rulesA
                    }

                    DashboardChannelCard {
                        Layout.fillWidth: true
                        channel: "B"
                        modules: home.modulesB
                        rules: home.rulesB
                    }
                }

                ColumnLayout {
                    objectName: "homeDashboardRightColumn"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.alignment: Qt.AlignTop
                    spacing: Metrics.cardSpacing

                    DashboardPluginCard {
                        Layout.fillWidth: true
                        pluginCenterPage: page.pagePluginCenter
                    }

                    DashboardTriggerFeed {
                        Layout.fillWidth: true
                    }

                    DashboardWaveCard {
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }
}
