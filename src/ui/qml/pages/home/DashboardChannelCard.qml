import QtQuick
import QtQuick.Layouts

// 通道卡：已挂载模块与直连规则（只读绑定），入口仍在页头的模块管理与规则编辑器。
// 强度调节与波形选择已移出主页，卡内留一行说明指向后续入口。
GlassCard {
    id: root

    property string channel: "A"
    property var modules: []
    property var rules: []

    objectName: "dashboardChannel" + root.channel + "Card"
    title: qsTr("通道 %1 · 模块与规则").arg(root.channel)

    // 无障碍：整卡作为 Pane 暴露（UIA 与辅助技术能读到卡片范围与标题）
    Accessible.role: Accessible.Pane
    Accessible.name: root.title

    // 分组之间用 groupSpacing 分隔，组内保持「小标题 + 留白」，不画内层描边
    Column {
        id: groups

        width: parent.width
        spacing: ComponentStyle.groupSpacing

        Column {
            id: moduleGroup

            width: groups.width
            spacing: ComponentStyle.groupTitleGap

            Text {
                text: qsTr("已挂载模块")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                font.bold: true
            }

            Text {
                width: moduleGroup.width
                visible: root.modules.length === 0
                text: qsTr("未挂载模块")
                color: Theme.textMuted
                font.pixelSize: Typography.fontBody
            }

            // 条目再多区块高度也有界：限高 + 裁剪 + 滚动，卡片不会被列表撑出可视区
            Flickable {
                id: moduleFlick

                width: moduleGroup.width
                height: Math.min(moduleList.implicitHeight, ComponentStyle.panelBlockMaxHeight)
                contentWidth: width
                contentHeight: moduleList.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                Column {
                    id: moduleList

                    width: moduleFlick.width
                    spacing: Metrics.spacingSm

                    Repeater {
                        model: root.modules

                        delegate: RowLayout {
                            required property var modelData

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

        Column {
            id: ruleGroup

            width: groups.width
            spacing: ComponentStyle.groupTitleGap

            Text {
                text: qsTr("规则")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                font.bold: true
            }

            Text {
                width: ruleGroup.width
                visible: root.rules.length === 0
                text: qsTr("无直连规则")
                color: Theme.textMuted
                font.pixelSize: Typography.fontBody
            }

            Flickable {
                id: ruleFlick

                width: ruleGroup.width
                height: Math.min(ruleList.implicitHeight, ComponentStyle.panelBlockMaxHeight)
                contentWidth: width
                contentHeight: ruleList.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                Column {
                    id: ruleList

                    width: ruleFlick.width
                    spacing: Metrics.spacingSm

                    Repeater {
                        model: root.rules

                        delegate: RowLayout {
                            required property var modelData

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
                                text: modelData.value === undefined || modelData.value === null ? "--" : modelData.value
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

    Text {
        objectName: "dashboardChannel" + root.channel + "Hint"
        width: parent.width
        text: qsTr("强度调节与波形选择将在迷你窗与波形设置页提供")
        color: Theme.textMuted
        font.pixelSize: Typography.fontBody
        wrapMode: Text.WordWrap
    }
}
