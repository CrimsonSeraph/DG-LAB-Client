import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// 配置页：连接（IP/端口/二维码）、波形、主题、日志，以及规则编辑器入口。
// 交互约定：本文件不写任何信号处理器；交互控件由 C++（UiConnector）按 objectName 连接。
Item {
    id: page

    objectName: "configPage"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin
        spacing: Metrics.cardSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            Text {
                text: qsTr("配置")
                color: Theme.textPrimary
                font.pixelSize: Typography.fontTitle
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "configRuleEditorButton"
                text: qsTr("规则编辑器")
            }
        }

        // ------------------------------------------------------------ 连接
        GlassCard {
            Layout.fillWidth: true
            title: qsTr("连接")

            RowLayout {
                width: parent.width
                spacing: Metrics.spacingMd

                Text {
                    text: qsTr("地址")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                }

                TextField {
                    id: ipField

                    objectName: "configIpField"
                    text: device.ip
                    Layout.preferredWidth: 180
                }

                Text {
                    text: qsTr("端口")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                }

                TextField {
                    id: portField

                    objectName: "configPortField"
                    text: device.port
                    Layout.preferredWidth: 100
                    validator: IntValidator {
                        bottom: 1
                        top: 65535
                    }
                }

                AppButton {
                    objectName: "configConnectButton"
                    text: device.connected ? qsTr("断开") : qsTr("连接")
                    primary: !device.connected
                    enabled: !device.connecting
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            Column {
                visible: device.connected
                spacing: Metrics.spacingSm

                Image {
                    objectName: "configQrImage"
                    width: 200
                    height: 200
                    source: device.qrImageUrl
                    sourceSize.width: 200
                    sourceSize.height: 200
                    fillMode: Image.PreserveAspectFit
                }

                Text {
                    width: 420
                    text: device.pairingUrl
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontCaption
                    elide: Text.ElideMiddle
                }

                Text {
                    text: device.v3Paired ? qsTr("APP 已配对") : (device.v4Attached ? qsTr("V4 已接入，等待设备上报") : qsTr("等待 APP 扫码配对"))
                    color: (device.v3Paired || device.v4Attached) ? Theme.accent : Theme.textMuted
                    font.pixelSize: Typography.fontCaption
                }
            }
        }

        // ------------------------------------------------------------ 波形
        GlassCard {
            Layout.fillWidth: true
            title: qsTr("波形")

            Column {
                width: parent.width
                spacing: Metrics.spacingMd

                RowLayout {
                    width: parent.width
                    spacing: Metrics.spacingMd

                    Text {
                        text: qsTr("目标通道")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                    }

                    AppButton {
                        objectName: "configWaveChannelAButton"
                        text: "A"
                        primary: waveBridge.targetChannel === "A"
                    }

                    AppButton {
                        objectName: "configWaveChannelBButton"
                        text: "B"
                        primary: waveBridge.targetChannel === "B"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Text {
                        text: qsTr("当前：A = %1 ｜ B = %2").arg(waveBridge.currentA.length > 0 ? waveBridge.currentA : qsTr("未选择")).arg(waveBridge.currentB.length > 0 ? waveBridge.currentB : qsTr("未选择"))
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontCaption
                        elide: Text.ElideRight
                    }
                }

                RowLayout {
                    width: parent.width
                    spacing: Metrics.spacingLg

                    Column {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingXs

                        Text {
                            text: qsTr("通道 A")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontSmall
                        }

                        WavePreview {
                            width: parent.width
                            height: ComponentStyle.wavePreviewHeight
                            points: waveBridge.pointsA
                        }
                    }

                    Column {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingXs

                        Text {
                            text: qsTr("通道 B")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontSmall
                        }

                        WavePreview {
                            width: parent.width
                            height: ComponentStyle.wavePreviewHeight
                            points: waveBridge.pointsB
                        }
                    }
                }

                RowLayout {
                    width: parent.width
                    spacing: Metrics.spacingMd

                    AppButton {
                        objectName: "configWaveSelectButton"
                        text: qsTr("选择波形")
                    }

                    AppButton {
                        objectName: "configWaveCreateButton"
                        text: qsTr("创建波形")
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // ------------------------------------------------------------ 主题
        GlassCard {
            Layout.fillWidth: true
            title: qsTr("主题")

            RowLayout {
                width: parent.width
                spacing: Metrics.spacingLg

                Text {
                    text: Theme.displayName
                    color: Theme.textPrimary
                    font.pixelSize: Typography.fontBodyLarge
                    font.bold: true
                }

                Rectangle {
                    Layout.preferredWidth: ComponentStyle.swatchMinWidth
                    Layout.preferredHeight: ComponentStyle.swatchHeight
                    radius: Metrics.radiusSm
                    color: Theme.primary
                    border.width: Metrics.borderWidth
                    border.color: Theme.border

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }

                Text {
                    text: qsTr("主色 %1").arg(Theme.primary)
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontCaption
                }

                Rectangle {
                    Layout.preferredWidth: ComponentStyle.swatchMinWidth
                    Layout.preferredHeight: ComponentStyle.swatchHeight
                    radius: Metrics.radiusSm
                    color: Theme.secondary
                    border.width: Metrics.borderWidth
                    border.color: Theme.border

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }

                Text {
                    text: qsTr("副色 %1").arg(Theme.secondary)
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontCaption
                }

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    objectName: "configThemeSelectButton"
                    text: qsTr("选择主题")
                }

                AppButton {
                    objectName: "configThemeCustomButton"
                    text: qsTr("自定义主题")
                    primary: Theme.custom
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }

    // -------------------------------------------------------- 预设主题对话框
    Dialog {
        id: themePresetDialog

        objectName: "themePresetDialog"
        title: qsTr("选择主题")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 560
        height: 460
        padding: Metrics.spacingMd

        ListView {
            id: presetList

            objectName: "themePresetList"
            anchors.fill: parent
            clip: true
            model: Theme.presets
            spacing: Metrics.spacingXs

            delegate: Item {
                width: presetList.width
                height: 54

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: Metrics.radiusSm
                    color: Theme.surfaceAlt
                    border.width: Metrics.borderWidth
                    border.color: Theme.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Metrics.spacingMd
                        spacing: Metrics.spacingMd

                        Rectangle {
                            Layout.preferredWidth: 40
                            Layout.preferredHeight: 20
                            radius: Metrics.radiusXs
                            color: modelData.primary
                            border.width: Metrics.borderWidth
                            border.color: Theme.divider
                        }

                        Rectangle {
                            Layout.preferredWidth: 40
                            Layout.preferredHeight: 20
                            radius: Metrics.radiusXs
                            color: modelData.secondary
                            border.width: Metrics.borderWidth
                            border.color: Theme.divider
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: Theme.textPrimary
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }

                        Text {
                            text: modelData.mode
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontCaption
                        }
                    }
                }

                // C++ 侧扫描该热区并按 mode 应用主题（QML 不写 onClicked）
                MouseArea {
                    objectName: "themePresetClick"
                    anchors.fill: parent
                    property string mode: modelData.mode
                }
            }
        }
    }

    // -------------------------------------------------------- 自定义主题对话框
    Dialog {
        id: customThemeDialog

        objectName: "customThemeDialog"
        title: qsTr("自定义主题")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 420
        height: 260
        padding: Metrics.spacingLg

        property color customPrimary: Theme.primary
        property color customSecondary: Theme.secondary

        ColumnLayout {
            anchors.fill: parent
            spacing: Metrics.spacingLg

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingMd

                Text {
                    text: qsTr("主色")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                    Layout.preferredWidth: 48
                }

                Rectangle {
                    Layout.preferredWidth: ComponentStyle.swatchMinWidth
                    Layout.preferredHeight: ComponentStyle.swatchHeight
                    radius: Metrics.radiusSm
                    color: customThemeDialog.customPrimary
                    border.width: Metrics.borderWidth
                    border.color: Theme.border
                }

                Text {
                    Layout.fillWidth: true
                    text: customThemeDialog.customPrimary
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontCaption
                }

                AppButton {
                    objectName: "customThemePrimaryButton"
                    text: qsTr("选色")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingMd

                Text {
                    text: qsTr("副色")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                    Layout.preferredWidth: 48
                }

                Rectangle {
                    Layout.preferredWidth: ComponentStyle.swatchMinWidth
                    Layout.preferredHeight: ComponentStyle.swatchHeight
                    radius: Metrics.radiusSm
                    color: customThemeDialog.customSecondary
                    border.width: Metrics.borderWidth
                    border.color: Theme.border
                }

                Text {
                    Layout.fillWidth: true
                    text: customThemeDialog.customSecondary
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontCaption
                }

                AppButton {
                    objectName: "customThemeSecondaryButton"
                    text: qsTr("选色")
                }
            }

            Item {
                Layout.fillHeight: true
            }

            AppButton {
                objectName: "customThemeSaveButton"
                Layout.fillWidth: true
                text: qsTr("保存并应用")
                primary: true
            }
        }
    }

    ColorDialog {
        id: primaryColorDialog
        objectName: "primaryColorDialog"
        title: qsTr("选择主色")
        color: customThemeDialog.customPrimary
    }

    ColorDialog {
        id: secondaryColorDialog
        objectName: "secondaryColorDialog"
        title: qsTr("选择副色")
        color: customThemeDialog.customSecondary
    }
}
