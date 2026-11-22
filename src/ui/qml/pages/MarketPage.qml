import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 插件市场：registry 清单驱动的插件安装与更新入口。
// 清单项的三态（install / update / installed）与安装进度都来自 C++ 桥接 market；
// 交互约定：本文件不写信号处理器，刷新与卡片上的操作按钮都由 C++（UiConnector）按 objectName 连接。
Item {
    id: page

    objectName: "marketPage"

    ScrollView {
        id: scrollView

        objectName: "marketScrollView"
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        padding: Metrics.pageMargin
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: Metrics.cardSpacing

            PageHeader {
                objectName: "marketHeader"
                Layout.fillWidth: true
                title: qsTr("插件市场")
                subtitle: market.loading
                    ? qsTr("正在获取插件清单…")
                    : qsTr("清单 %1 个插件").arg(market.plugins.length)

                AppButton {
                    objectName: "marketRefreshButton"
                    text: market.loading ? qsTr("刷新中…") : qsTr("刷新清单")
                    enabled: !market.loading
                    primary: true
                }
            }

            GlassCard {
                Layout.fillWidth: true
                title: qsTr("插件清单")

                ColumnLayout {
                    width: parent.width
                    spacing: ComponentStyle.groupTitleGap

                    Text {
                        objectName: "marketSourceText"
                        Layout.fillWidth: true
                        text: market.sourceUrl
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideMiddle
                    }

                    Text {
                        objectName: "marketUpdatedText"
                        Layout.fillWidth: true
                        text: market.updatedAt.length > 0
                            ? qsTr("清单时间 %1").arg(market.updatedAt)
                            : qsTr("尚未获取清单")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }

                    Text {
                        objectName: "marketLoadingText"
                        Layout.fillWidth: true
                        visible: market.loading
                        text: qsTr("正在获取插件清单…")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                    }

                    Text {
                        objectName: "marketErrorText"
                        Layout.fillWidth: true
                        visible: market.lastError.length > 0
                        text: market.lastError
                        color: Theme.danger
                        font.pixelSize: Typography.fontBody
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        objectName: "marketEmptyText"
                        Layout.fillWidth: true
                        visible: !market.loading && market.lastError.length === 0 && market.plugins.length === 0
                        text: qsTr("清单里暂时没有插件，可稍后刷新重试")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                    }

                    // 与插件中心同一套网格：固定两列、行数不限，卡片各按统一高度顶对齐；
                    // 窄窗口下两列照旧（不对宽度分档），与插件中心的行为一致
                    GridLayout {
                        objectName: "marketList"
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: Metrics.cardSpacing
                        rowSpacing: Metrics.cardSpacing

                        Repeater {
                            model: market.plugins

                            delegate: AppCard {
                                objectName: "marketCard"
                                Layout.fillWidth: true
                                // GridLayout 默认把同行单元格拉齐，短卡会被撑出空白
                                Layout.alignment: Qt.AlignTop
                                Layout.preferredHeight: ComponentStyle.pluginCardHeight
                                title: modelData.name
                                subtitle: modelData.description
                                banner: modelData.banner
                                tags: modelData.tags
                                // 失败原因优先：依赖缺失与校验失败要在卡片上就地可见
                                status: modelData.error.length > 0 ? modelData.error : modelData.statusText

                                AppButton {
                                    objectName: "marketActionButton"
                                    // 卡片由 Repeater 生成：插件标识写在属性上，由 C++ 侧从 sender 读取
                                    property string pluginId: modelData.id
                                    text: modelData.actionText
                                    enabled: modelData.actionEnabled
                                    primary: modelData.status === "install"
                                    danger: modelData.status === "update"
                                    outline: modelData.status === "update"
                                }

                                ProgressBar {
                                    id: installProgress

                                    objectName: "marketProgress"
                                    width: Metrics.inputMinWidth
                                    height: Metrics.spacingLg
                                    visible: modelData.status === "installing"
                                    value: modelData.progress

                                    background: Rectangle {
                                        implicitHeight: Metrics.spacingSm
                                        radius: Metrics.radiusXs
                                        color: Theme.surfaceAlt
                                        border.width: Metrics.borderWidth
                                        border.color: Theme.border
                                    }

                                    contentItem: Item {
                                        Rectangle {
                                            width: installProgress.visualPosition * installProgress.availableWidth
                                            height: installProgress.availableHeight
                                            radius: Metrics.radiusXs
                                            color: Theme.accent
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
