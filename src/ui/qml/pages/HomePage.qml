import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 主页看板：设备状态、通道与波形逐行配对，已启用插件与近期触发在同行右侧滚动。
// 三行结构：设备状态 | 已启用插件 + 近期触发（同高，超出时右列内部滚动）；
//          通道 A | 通道 A 波形；通道 B | 通道 B 波形。每行左右等高（Layout.fillHeight 拉满格高），
//          窄窗口整体退化为单列，单列时等高约束自然失效（行内只有一个孩子）。
// 强度增减与波形选择已移出本页（迷你窗 / 波形设置页负责），卡内只留只读数值与说明。
// 交互约定：页内导航按钮用 app.navigate，其余交互控件仍由 C++（UiConnector）按 objectName 连接。
// 页头操作区给「迷你窗」与「规则编辑器」两个入口：插件中心经左侧导航进入，本页不提供插件中心快捷入口。
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

                // 操作区从左到右：迷你窗入口（打开无边框置顶小窗）、规则编辑器
                AppButton {
                    objectName: "homeMiniWindowEntryButton"
                    text: qsTr("迷你窗")
                }

                AppButton {
                    objectName: "homeRuleEditorButton"
                    text: qsTr("规则编辑器")
                }
            }

            // 看板栅格：两列时按「设备 | 插件+触发」「通道 A | A 波形」「通道 B | B 波形」逐行排列，
            // 窄窗口 columns 退为 1，同一批子项按顺序变成单列，页内顺序与两列时一致。
            // 成对等高靠 Layout.fillHeight：同行格高取两个孩子期望高度中的较大者，两个孩子都拉满格高，
            // 因此不会出现「左边卡高、右边卡矮一截」；单列时行内只有一个孩子，格高即它的期望高度，行为不变。
            GridLayout {
                objectName: "homeDashboardGrid"
                Layout.fillWidth: true
                columns: page.singleColumn ? 1 : 2
                columnSpacing: Metrics.cardSpacing
                rowSpacing: Metrics.cardSpacing

                // 第一行左：设备状态卡
                DashboardDeviceCard {
                    // 无障碍：卡容器本身没有 a11y 角色，在实例上补 Pane 与标题（UIA / 读屏可读到整卡范围）
                    Accessible.role: Accessible.Pane
                    Accessible.name: qsTr("设备状态")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.fillHeight: true
                }

                // 第一行右：已启用插件 + 近期触发纵向排列，高度跟随同行的设备状态卡；
                // 两卡内容超出该高度时在这里内部滚动（contentHeight 取自下方 ColumnLayout 位置器）
                Flickable {
                    id: rightStack

                    objectName: "homeDashboardRightStack"
                    // 无障碍：整块右列作为 Pane 暴露（UIA / 读屏能读到这块区域与标题）
                    Accessible.role: Accessible.Pane
                    Accessible.name: qsTr("已启用插件与近期触发")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.fillHeight: !page.singleColumn
                    // 两列时不给期望高度，行高完全由设备状态卡决定；单列时没有同行伙伴，按内容自然高度
                    Layout.preferredHeight: page.singleColumn ? rightStackContent.implicitHeight : 0
                    contentWidth: width
                    // 内容不足一屏时按可视高度铺满（卡片内部的空白由下方 fillHeight 分配到近期触发卡），
                    // 内容超出时才按内容高度算，滚动条据此出现
                    contentHeight: Math.max(rightStackContent.implicitHeight, rightStack.height)
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
                        // 与 contentHeight 同一口径：内容短于可视区时撑到可视高度，
                        // 多出来的高度由下面的近期触发卡吸收，组合高度正好等于设备状态卡
                        height: Math.max(implicitHeight, rightStack.height)
                        spacing: Metrics.cardSpacing

                        DashboardPluginCard {
                            Layout.fillWidth: true
                            pluginCenterPage: page.pagePluginCenter
                        }

                        DashboardTriggerFeed {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                    }
                }

                // 第二行：通道 A 卡与其波形卡同行，两张卡都拉满格高（较高的一张决定行高）
                DashboardChannelCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.fillHeight: true
                    channel: "A"
                    modules: home.modulesA
                    rules: home.rulesA
                }

                DashboardWaveCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.fillHeight: true
                    channel: "A"
                }

                // 第三行：通道 B 卡与其波形卡同行，同上
                DashboardChannelCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.fillHeight: true
                    channel: "B"
                    modules: home.modulesB
                    rules: home.rulesB
                }

                DashboardWaveCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.fillHeight: true
                    channel: "B"
                }
            }
        }
    }
}
