import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 模块页：统一查询周期、插件/内置模块卡片、数值展示弹窗。
// 交互约定：本文件不写任何信号处理器；交互控件由 C++（UiConnector）按 objectName 连接。
Item {
    id: page

    objectName: "modulePage"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin
        spacing: Metrics.cardSpacing

        Text {
            text: qsTr("模块")
            color: Theme.textPrimary
            font.pixelSize: Typography.fontTitle
            font.bold: true
        }

        GlassCard {
            Layout.fillWidth: true
            title: qsTr("查询周期")

            RowLayout {
                width: parent.width
                spacing: Metrics.spacingMd

                Text {
                    text: qsTr("统一查询周期")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                }

                AppComboBox {
                    id: periodCombo

                    objectName: "modulePeriodCombo"
                    model: moduleBridge.periodOptions
                    textRole: "text"
                    Layout.preferredWidth: 160
                }

                AppButton {
                    objectName: "modulePeriodApplyButton"
                    text: qsTr("应用到全部数值")
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    text: qsTr("当前基准周期 %1ms").arg(moduleBridge.basePeriodMs)
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontCaption
                }
            }
        }

        GlassCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: qsTr("模块")

            Flow {
                id: moduleCardFlow

                objectName: "moduleCardFlow"
                width: parent.width
                spacing: Metrics.cardSpacing

                Repeater {
                    model: moduleBridge.modules

                    delegate: Rectangle {
                        width: 232
                        height: 118
                        radius: Metrics.radiusMd
                        color: Theme.surfaceAlt
                        border.width: Metrics.borderWidth
                        border.color: Theme.border
                        opacity: modelData.enabled ? 1.0 : 0.65

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Metrics.spacingLg
                            spacing: Metrics.spacingXs

                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                color: Theme.textPrimary
                                font.pixelSize: Typography.fontBody
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Text {
                                text: modelData.stateText
                                color: Theme.textSecondary
                                font.pixelSize: Typography.fontSmall
                            }

                            Text {
                                Layout.fillWidth: true
                                text: {
                                    if (modelData.kind !== "plugin") {
                                        return qsTr("最小周期 %1ms").arg(modelData.periodMs);
                                    }
                                    if (modelData.enabled) {
                                        return qsTr("最小周期 %1ms").arg(modelData.periodMs);
                                    }
                                    return modelData.error.length > 0 ? modelData.error : qsTr("启用后将加载");
                                }
                                color: Theme.textMuted
                                font.pixelSize: Typography.fontCaption
                                elide: Text.ElideRight
                                wrapMode: Text.WordWrap
                            }

                            Item {
                                Layout.fillHeight: true
                            }
                        }

                        // 点击卡片查看数值（未加载的插件不可点击）
                        MouseArea {
                            objectName: "moduleCardClick"
                            anchors.fill: parent
                            enabled: modelData.enabled
                            property string moduleName: modelData.name
                        }

                        AppButton {
                            objectName: "pluginToggleClick"
                            property string fileName: modelData.fileName
                            visible: modelData.kind === "plugin"
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: Metrics.spacingSm
                            text: modelData.enabled ? qsTr("禁用") : qsTr("启用")
                        }
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------ 数值展示
    Dialog {
        id: moduleValuesDialog

        objectName: "moduleValuesDialog"
        title: qsTr("模块数值 - %1").arg(moduleBridge.selectedModule)
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 540
        height: 460
        padding: Metrics.spacingMd

        background: Rectangle {
            radius: Metrics.radiusMd
            color: Theme.surface
            border.width: Metrics.borderWidth
            border.color: Theme.border
        }

        ListView {
            anchors.fill: parent
            clip: true
            model: moduleBridge.selectedValues
            spacing: Metrics.spacingXs

            delegate: Rectangle {
                width: ListView.view.width
                height: 44
                radius: Metrics.radiusSm
                color: Theme.surfaceAlt
                border.width: Metrics.borderWidth
                border.color: Theme.divider

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Metrics.spacingMd
                    spacing: Metrics.spacingMd

                    Text {
                        Layout.fillWidth: true
                        text: modelData.name + "  (" + modelData.id + ")"
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontSmall
                        elide: Text.ElideRight
                    }

                    Text {
                        text: modelData.value === undefined ? "--" : modelData.value
                        color: Theme.accent
                        font.pixelSize: Typography.fontBody
                        font.bold: true
                    }

                    Text {
                        text: (modelData.min === undefined ? "NULL" : modelData.min) + " / " + (modelData.max === undefined ? "NULL" : modelData.max)
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontCaption
                    }

                    Text {
                        text: modelData.periodText
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontCaption
                    }
                }
            }
        }
    }
}
