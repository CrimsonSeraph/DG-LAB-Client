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

                    // 卡片底部行：选择 / 创建波形右对齐
                    RowLayout {
                        width: parent.width
                        spacing: Metrics.spacingMd

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

            Item {
                Layout.preferredHeight: Metrics.pageMargin
            }
        }
    }
}
