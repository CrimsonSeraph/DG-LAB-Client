import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 首页通道面板：强度 / 模块 / 规则 / 波形 / 启停。
//
// 交互约定：QML 不写任何信号处理器，交互控件都有稳定的 objectName
// （channel + 通道字母 + 功能名），由 C++（UiConnector）按名连接。
GlassCard {
    id: root

    property string channel: "A"
    property int strength: 0
    property int limit: 200
    property bool running: false
    property var modules: []
    property var rules: []
    property string waveName: "未选择"
    property var wavePoints: []
    // 通道输出状态（home.channelAState / channelBState）：runningNoOutput 表示已启动但无可用输出
    readonly property string outputState: root.channel === "A" ? home.channelAState : home.channelBState

    title: qsTr("通道 %1").arg(root.channel)

    // 卡片宽度只由父布局（权重）决定：把隐含宽度钉成常量，切断
    // 「卡片内文本长度 -> 隐含宽度 -> 卡片实际宽度」的回传链路。
    implicitWidth: 0

    Column {
        id: content

        width: parent.width
        spacing: Metrics.spacingMd

        // 输出不可用警示：通道在跑，但没有任何可用的下发链路（指令会被丢弃）
        Text {
            width: parent.width
            visible: root.outputState === "runningNoOutput"
            text: qsTr("已启动，但当前没有可用输出（未连接设备）")
            color: Theme.warning
            font.pixelSize: Typography.fontBody
            wrapMode: Text.WordWrap
        }

        // 强度
        Rectangle {
            id: strengthBlock

            width: parent.width
            height: strengthColumn.implicitHeight + Metrics.spacingLg * 2
            color: Theme.surfaceAlt
            radius: Metrics.radiusMd
            border.width: Metrics.borderWidth
            border.color: Theme.border

            RowLayout {
                id: strengthColumn

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Metrics.spacingLg
                spacing: Metrics.spacingMd

                Text {
                    text: qsTr("强度")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                }

                AppSpinBox {
                    id: strengthSpin

                    objectName: "channel" + root.channel + "StrengthSpin"
                    from: 0
                    to: Math.max(1, root.limit)
                    editable: true
                    Layout.preferredWidth: ComponentStyle.strengthValueWidth

                    // 用 Binding 回灌目标值：用户编辑不会破坏与 home.strength 的同步。
                    // when 条件保证用户正在键入（获得焦点）时不回灌，避免覆盖输入中间值；
                    // RestoreNone 保证条件关闭时不清掉控件当前值（默认行为会退回构造时的 0）。
                    // 失焦后 valueModified 已把数值写回 home.strength，回灌等于用户输入值。
                    Binding {
                        target: strengthSpin
                        property: "value"
                        value: root.strength
                        when: !strengthSpin.activeFocus
                        restoreMode: Binding.RestoreNone
                    }
                }

                AppButton {
                    objectName: "channel" + root.channel + "StrengthDecrease"
                    text: qsTr("－")
                }

                AppButton {
                    objectName: "channel" + root.channel + "StrengthIncrease"
                    text: qsTr("＋")
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    text: qsTr("上限 %1").arg(root.limit)
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                }
            }
        }

        // 模块
        Rectangle {
            id: moduleBlock

            width: parent.width
            height: moduleColumn.implicitHeight + Metrics.spacingLg * 2
            color: Theme.surfaceAlt
            radius: Metrics.radiusMd
            border.width: Metrics.borderWidth
            border.color: Theme.border

            Column {
                id: moduleColumn

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Metrics.spacingLg
                spacing: Metrics.spacingSm

                Text {
                    text: qsTr("模块")
                    color: Theme.textPrimary
                    font.pixelSize: Typography.fontBody
                    font.bold: true
                }

                Text {
                    visible: root.modules.length === 0
                    text: qsTr("未挂载模块")
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                }

                // 条目再多区块高度也有界：内容区限高 + 裁剪 + 滚动，卡片里的「启动通道」始终可见
                Flickable {
                    id: moduleListFlick

                    width: parent.width
                    height: Math.min(moduleList.implicitHeight, ComponentStyle.panelBlockMaxHeight)
                    contentWidth: width
                    contentHeight: moduleList.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true

                    Column {
                        id: moduleList

                        width: moduleListFlick.width
                        spacing: Metrics.spacingSm

                        Repeater {
                            model: root.modules

                            delegate: RowLayout {
                                width: moduleList.width
                                spacing: Metrics.spacingSm

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: Theme.textSecondary
                                    font.pixelSize: Typography.fontBody
                                    elide: Text.ElideRight
                                }

                                Text {
                                    text: qsTr("%1ms").arg(modelData.periodMs)
                                    color: Theme.textMuted
                                    font.pixelSize: Typography.fontBody
                                }
                            }
                        }
                    }
                }
            }
        }

        // 规则
        Rectangle {
            id: ruleBlock

            width: parent.width
            height: ruleColumn.implicitHeight + Metrics.spacingLg * 2
            color: Theme.surfaceAlt
            radius: Metrics.radiusMd
            border.width: Metrics.borderWidth
            border.color: Theme.border

            Column {
                id: ruleColumn

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Metrics.spacingLg
                spacing: Metrics.spacingSm

                Text {
                    text: qsTr("规则")
                    color: Theme.textPrimary
                    font.pixelSize: Typography.fontBody
                    font.bold: true
                }

                Text {
                    visible: root.rules.length === 0
                    text: qsTr("无直连规则")
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                }

                // 同模块区块：限高 + 裁剪 + 滚动，规则条目增多不会把卡片撑出可视区
                Flickable {
                    id: ruleListFlick

                    width: parent.width
                    height: Math.min(ruleList.implicitHeight, ComponentStyle.panelBlockMaxHeight)
                    contentWidth: width
                    contentHeight: ruleList.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true

                    Column {
                        id: ruleList

                        width: ruleListFlick.width
                        spacing: Metrics.spacingSm

                        Repeater {
                            model: root.rules

                            delegate: RowLayout {
                                width: ruleList.width
                                spacing: Metrics.spacingSm

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: Theme.textSecondary
                                    font.pixelSize: Typography.fontBody
                                    elide: Text.ElideRight
                                }

                                Text {
                                    text: modelData.value === undefined ? "--" : modelData.value
                                    color: Theme.accent
                                    font.pixelSize: Typography.fontBody
                                    font.bold: true
                                }
                            }
                        }
                    }
                }
            }
        }

        // 波形
        Rectangle {
            id: waveBlock

            width: parent.width
            height: waveColumn.implicitHeight + Metrics.spacingLg * 2
            color: Theme.surfaceAlt
            radius: Metrics.radiusMd
            border.width: Metrics.borderWidth
            border.color: Theme.border

            Column {
                id: waveColumn

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Metrics.spacingLg
                spacing: Metrics.spacingSm

                Text {
                    text: qsTr("波形")
                    color: Theme.textPrimary
                    font.pixelSize: Typography.fontBody
                    font.bold: true
                }

                WavePreview {
                    width: parent.width
                    height: ComponentStyle.wavePreviewHeight
                    points: root.wavePoints
                }

                RowLayout {
                    width: parent.width
                    spacing: Metrics.spacingMd

                    Text {
                        Layout.fillWidth: true
                        text: root.waveName
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }

                    AppButton {
                        objectName: "channel" + root.channel + "SelectWaveButton"
                        text: qsTr("选择波形")
                    }
                }
            }
        }

        // 启停
        AppButton {
            objectName: "channel" + root.channel + "StartButton"
            width: parent.width
            text: root.running ? qsTr("关闭通道") : qsTr("启动通道")
            primary: !root.running
            danger: root.running
        }
    }
}
