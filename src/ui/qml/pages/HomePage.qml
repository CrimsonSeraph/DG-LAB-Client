import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 主页看板：设备状态、通道与波形逐行配对，已启用插件与近期触发在同行右侧滚动。
// 三行结构：设备状态 | 已启用插件 + 近期触发（同高，超出时右列内部滚动）；
//          通道 A | 通道 A 波形；通道 B | 通道 B 波形。窄窗口整体退化为单列。
// 强度增减与波形选择已移出本页（迷你窗 / 波形设置页负责），卡内只留只读数值与说明。
// 交互约定：页内导航按钮用 app.navigate，其余交互控件仍由 C++（UiConnector）按 objectName 连接。
// 页头只留「规则编辑器」：插件中心经左侧导航进入，本页不再提供插件中心快捷入口。
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
                    objectName: "homeRuleEditorButton"
                    text: qsTr("规则编辑器")
                }
            }

            // 迷你窗入口：打开无边框置顶的常驻小窗（可拖动 / 锁定 / 隐藏）
            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingMd

                AppButton {
                    objectName: "homeMiniWindowEntryButton"
                    text: qsTr("迷你窗")
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            // 看板栅格：两列时按「设备 | 插件+触发」「通道 A | A 波形」「通道 B | B 波形」逐行排列，
            // 窄窗口 columns 退为 1，同一批子项按顺序变成单列，页内顺序与两列时一致
            GridLayout {
                objectName: "homeDashboardGrid"
                Layout.fillWidth: true
                columns: page.singleColumn ? 1 : 2
                columnSpacing: Metrics.cardSpacing
                rowSpacing: Metrics.cardSpacing

                // 第一行左：设备状态卡，行高由它决定
                DashboardDeviceCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.alignment: Qt.AlignTop
                }

                // 第一行右：已启用插件 + 近期触发纵向排列，高度跟随同行的设备状态卡；
                // 两卡内容超出该高度时在这里内部滚动（contentHeight 取自下方 ColumnLayout 位置器）
                Flickable {
                    id: rightStack

                    objectName: "homeDashboardRightStack"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.fillHeight: !page.singleColumn
                    // 两列时不给期望高度，行高完全由设备状态卡决定；单列时没有同行伙伴，按内容自然高度
                    Layout.preferredHeight: page.singleColumn ? rightStackContent.implicitHeight : 0
                    contentWidth: width
                    contentHeight: rightStackContent.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.vertical: ScrollBar {
                        id: rightStackBar

                        // 只在内容确实高出可视区时出现：单列时高度按内容给足，不该留下一条空滚动条
                        policy: rightStack.contentHeight > rightStack.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                        width: Metrics.spacingSm
                        padding: 0

                        contentItem: Rectangle {
                            implicitWidth: Metrics.spacingXs
                            implicitHeight: Metrics.spacingXs
                            radius: Metrics.radiusXs
                            color: rightStackBar.pressed ? Theme.accent : Theme.textSecondary
                        }
                    }

                    ColumnLayout {
                        id: rightStackContent

                        width: rightStack.width
                        spacing: Metrics.cardSpacing

                        DashboardPluginCard {
                            Layout.fillWidth: true
                            pluginCenterPage: page.pagePluginCenter
                        }

                        DashboardTriggerFeed {
                            Layout.fillWidth: true
                        }
                    }
                }

                // 第二行：通道 A 卡与其波形卡同行
                DashboardChannelCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.alignment: Qt.AlignTop
                    channel: "A"
                    modules: home.modulesA
                    rules: home.rulesA
                }

                DashboardWaveCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.alignment: Qt.AlignTop
                    channel: "A"
                }

                // 第三行：通道 B 卡与其波形卡同行
                DashboardChannelCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.alignment: Qt.AlignTop
                    channel: "B"
                    modules: home.modulesB
                    rules: home.rulesB
                }

                DashboardWaveCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.alignment: Qt.AlignTop
                    channel: "B"
                }
            }
        }
    }
}
