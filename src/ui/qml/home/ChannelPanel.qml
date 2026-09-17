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

    title: qsTr("通道 %1").arg(root.channel)

    Column {
        id: content

        width: parent.width
        spacing: Metrics.spacingMd

        // ---------------------------------------------------------- 强度
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

                SpinBox {
                    id: strengthSpin

                    objectName: "channel" + root.channel + "StrengthSpin"
                    from: 0
                    to: Math.max(1, root.limit)
                    editable: true
                    Layout.preferredWidth: ComponentStyle.strengthValueWidth

                    // 用 Binding 回灌目标值：用户编辑不会破坏与 home.strength 的同步
                    Binding {
                        target: strengthSpin
                        property: "value"
                        value: root.strength
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
                    font.pixelSize: Typography.fontCaption
                }
            }
        }

        // ---------------------------------------------------------- 模块
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
                    font.pixelSize: Typography.fontCaption
                }

                Repeater {
                    model: root.modules

                    delegate: RowLayout {
                        width: moduleColumn.width
                        spacing: Metrics.spacingSm

                        Text {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontSmall
                            elide: Text.ElideRight
                        }

                        Text {
                            text: qsTr("%1ms").arg(modelData.periodMs)
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontCaption
                        }
                    }
                }
            }
        }

        // ---------------------------------------------------------- 规则
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
                    font.pixelSize: Typography.fontCaption
                }

                Repeater {
                    model: root.rules

                    delegate: RowLayout {
                        width: ruleColumn.width
                        spacing: Metrics.spacingSm

                        Text {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontSmall
                            elide: Text.ElideRight
                        }

                        Text {
                            text: modelData.value === undefined ? "--" : modelData.value
                            color: Theme.accent
                            font.pixelSize: Typography.fontSmall
                            font.bold: true
                        }
                    }
                }
            }
        }

        // ---------------------------------------------------------- 波形
        Rectangle {
            id: waveBlock

            width: parent.width
            height: waveRow.implicitHeight + Metrics.spacingLg * 2
            color: Theme.surfaceAlt
            radius: Metrics.radiusMd
            border.width: Metrics.borderWidth
            border.color: Theme.border

            RowLayout {
                id: waveRow

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Metrics.spacingLg
                spacing: Metrics.spacingMd

                Column {
                    Layout.fillWidth: true
                    spacing: 0

                    Text {
                        text: qsTr("波形")
                        color: Theme.textPrimary
                        font.pixelSize: Typography.fontBody
                        font.bold: true
                    }

                    Text {
                        text: root.waveName
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontCaption
                        elide: Text.ElideRight
                        width: parent.width
                    }
                }

                AppButton {
                    objectName: "channel" + root.channel + "SelectWaveButton"
                    text: qsTr("选择波形")
                }
            }
        }

        // ---------------------------------------------------------- 启停
        AppButton {
            objectName: "channel" + root.channel + "StartButton"
            width: parent.width
            text: root.running ? qsTr("关闭通道") : qsTr("启动通道")
            primary: !root.running
            danger: root.running
        }
    }
}
