import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 波形设置页：目标通道、A/B 波形预览与波形库 / 编辑器入口。
// 内容随窗口高度滚动（外层 ScrollView 撑满窗口），页内分组只用「小标题 + 留白」。
// 交互约定：本文件不写任何信号处理器；交互控件由 C++（UiConnector）按 objectName 连接。
Item {
    id: page

    objectName: "wavePage"

    ScrollView {
        id: scrollView

        objectName: "waveScrollView"
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        padding: Metrics.pageMargin
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: Metrics.cardSpacing

            PageHeader {
                Layout.fillWidth: true
                title: qsTr("波形设置")
                subtitle: qsTr("波形库管理与 A/B 通道波形指派 · 当前目标通道 %1").arg(waveBridge.targetChannel)
            }

            GlassCard {
                objectName: "waveChannelCard"
                Layout.fillWidth: true
                title: qsTr("通道波形")

                Column {
                    width: parent.width
                    spacing: ComponentStyle.groupSpacing

                    RowLayout {
                        width: parent.width
                        spacing: Metrics.spacingMd

                        Text {
                            text: qsTr("目标通道")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        // 选中态沿用 primary 语义：当前编辑目标通道整块强调，另一个保持默认表面色
                        AppButton {
                            objectName: "waveChannelAButton"
                            text: "A"
                            primary: waveBridge.targetChannel === "A"
                        }

                        AppButton {
                            objectName: "waveChannelBButton"
                            text: "B"
                            primary: waveBridge.targetChannel === "B"
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Text {
                            text: qsTr("当前：A = %1 ｜ B = %2").arg(waveBridge.currentA.length > 0 ? waveBridge.currentA : qsTr("未选择")).arg(waveBridge.currentB.length > 0 ? waveBridge.currentB : qsTr("未选择"))
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }
                    }

                    // A 与 B 平级：两列权重相同（preferredWidth 0 + fillWidth），
                    // 文本宽度与预览框隐含宽度都不参与列宽分配，只由布局权重等分。
                    RowLayout {
                        width: parent.width
                        spacing: Metrics.spacingLg

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            spacing: Metrics.spacingXs

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("通道 A")
                                color: Theme.textSecondary
                                font.pixelSize: Typography.fontBody
                                elide: Text.ElideRight
                            }

                            WavePreview {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 0
                                Layout.preferredHeight: ComponentStyle.wavePreviewHeight
                                points: waveBridge.pointsA
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            spacing: Metrics.spacingXs

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("通道 B")
                                color: Theme.textSecondary
                                font.pixelSize: Typography.fontBody
                                elide: Text.ElideRight
                            }

                            WavePreview {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 0
                                Layout.preferredHeight: ComponentStyle.wavePreviewHeight
                                points: waveBridge.pointsB
                            }
                        }
                    }

                    // 卡片底部行：导入 / 导出在左，选择 / 创建波形在右
                    RowLayout {
                        width: parent.width
                        spacing: Metrics.spacingMd

                        AppButton {
                            objectName: "waveImportButton"
                            text: qsTr("导入")
                        }

                        AppButton {
                            objectName: "waveExportButton"
                            text: qsTr("导出")
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        AppButton {
                            objectName: "waveSelectButton"
                            text: qsTr("选择波形")
                        }

                        AppButton {
                            objectName: "waveCreateButton"
                            text: qsTr("创建波形")
                        }
                    }
                }
            }

            // 波形标签：标签库分两层（内置带锁不可删、自定义可删），每个波形条目上方展示自己的标签
            GlassCard {
                objectName: "waveTagCard"
                Layout.fillWidth: true
                title: qsTr("波形标签")

                Column {
                    width: parent.width
                    spacing: ComponentStyle.groupSpacing

                    Text {
                        text: qsTr("标签库")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                    }

                    // 内置标签：代码常量，带锁标识，本页不提供删除入口
                    RowLayout {
                        width: parent.width
                        spacing: Metrics.spacingMd

                        Repeater {
                            model: waveBridge.builtinTags

                            delegate: RowLayout {
                                spacing: Metrics.spacingXs

                                AppIcon {
                                    icon: "f023"
                                    size: Typography.fontBody
                                    color: Theme.textMuted
                                }

                                StatusBadge {
                                    text: modelData
                                    level: 0
                                }
                            }
                        }

                        Text {
                            visible: waveBridge.builtinTags.length === 0
                            text: qsTr("暂无内置标签（等待官方标签清单）")
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    // 自定义标签：存 config/waves/tags.json，删除时一并从使用中的波形上移除
                    RowLayout {
                        width: parent.width
                        spacing: Metrics.spacingXs

                        Repeater {
                            model: waveBridge.customTags

                            delegate: StatusBadge {
                                text: modelData
                                level: 1
                            }
                        }

                        Text {
                            visible: waveBridge.customTags.length === 0
                            text: qsTr("还没有自定义标签：在波形条目的「编辑标签」里新建")
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    Text {
                        text: qsTr("波形列表")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                    }

                    Column {
                        id: waveTagWaveList

                        objectName: "waveTagWaveList"
                        width: parent.width
                        spacing: Metrics.spacingXs

                        Repeater {
                            model: waveBridge.waves

                            delegate: Rectangle {
                                width: waveTagWaveList.width
                                height: waveEntry.implicitHeight + Metrics.spacingMd * 2
                                radius: Metrics.radiusSm
                                color: Theme.surfaceAlt
                                border.width: Metrics.borderWidth
                                border.color: Theme.divider

                                Column {
                                    id: waveEntry
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.margins: Metrics.spacingMd
                                    spacing: Metrics.spacingXs

                                    // 标签行位于波形名上方：无标签时整行不占位
                                    Row {
                                        width: parent.width
                                        spacing: Metrics.spacingXs
                                        visible: modelData.tags.length > 0

                                        Repeater {
                                            model: modelData.tags

                                            delegate: StatusBadge {
                                                text: modelData
                                                level: 1
                                            }
                                        }
                                    }

                                    RowLayout {
                                        width: parent.width
                                        spacing: Metrics.spacingMd

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.name
                                            color: Theme.textPrimary
                                            font.pixelSize: Typography.fontBody
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            text: modelData.official
                                                ? qsTr("内置只读")
                                                : qsTr("%1 帧 · %2ms").arg(modelData.frameCount).arg(modelData.durationMs)
                                            color: Theme.textMuted
                                            font.pixelSize: Typography.fontBody
                                        }

                                        AppButton {
                                            objectName: "waveTagEditClick"
                                            property string waveName: modelData.name
                                            text: qsTr("编辑标签")
                                            enabled: !modelData.official
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item {
                Layout.preferredHeight: Metrics.pageMargin
            }
        }
    }
}
