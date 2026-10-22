import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// 配置页：连接（IP/端口/二维码）、波形、主题、日志，以及规则编辑器入口。
// 交互约定：本文件不写任何信号处理器；交互控件由 C++（UiConnector）按 objectName 连接。
Item {
    id: page

    objectName: "configPage"

    ScrollView {
        id: scrollView

        objectName: "configScrollView"
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
                title: qsTr("配置")
                subtitle: (device.pairingStage === 2 ? qsTr("已配对")
                        : (device.pairingStage === 1 ? qsTr("等待 APP 扫码配对") : qsTr("未启动")))
                    + " · " + (ble.connected ? qsTr("蓝牙已连接") : qsTr("蓝牙未连接"))

                AppButton {
                    objectName: "configRuleEditorButton"
                    text: qsTr("规则编辑器")
                }
            }

            // 连接
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

                    AppTextField {
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

                    AppTextField {
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

                // 连接状态：错误文案优先，其余按配对阶段显示
                Text {
                    width: parent.width
                    text: device.lastError.length > 0 ? device.lastError
                        : (device.pairingStage === 2 ? qsTr("已配对")
                        : (device.pairingStage === 1 ? qsTr("等待 APP 扫码配对") : qsTr("未启动")))
                    color: device.lastError.length > 0 ? Theme.danger
                        : (device.pairingStage === 2 ? Theme.success
                        : (device.pairingStage === 1 ? Theme.warning : Theme.textMuted))
                    font.pixelSize: Typography.fontBody
                    elide: Text.ElideRight
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
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideMiddle
                    }

                    Text {
                        text: device.v3Paired ? qsTr("APP 已配对") : (device.v4Attached ? qsTr("V4 已接入，等待设备上报") : qsTr("等待 APP 扫码配对"))
                        color: (device.v3Paired || device.v4Attached) ? Theme.accent : Theme.textMuted
                        font.pixelSize: Typography.fontBody
                    }
                }
            }

            // 波形
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

            // 主题
            GlassCard {
                Layout.fillWidth: true
                title: qsTr("主题")

                RowLayout {
                    width: parent.width
                    spacing: Metrics.spacingLg

                    Text {
                        text: Theme.displayName
                        color: Theme.textPrimary
                        font.pixelSize: Typography.fontSubtitle
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
                        font.pixelSize: Typography.fontBody
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
                        font.pixelSize: Typography.fontBody
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

            // 蓝牙直连
            GlassCard {
                Layout.fillWidth: true
                title: qsTr("蓝牙直连（郊狼 V3）")

                Column {
                    width: parent.width
                    spacing: Metrics.spacingMd

                    RowLayout {
                        width: parent.width
                        spacing: Metrics.spacingMd

                        Text {
                            text: ble.connected ? qsTr("已连接 %1").arg(ble.deviceName) : qsTr("未连接")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        Text {
                            visible: ble.battery >= 0
                            text: qsTr("电量 %1%").arg(ble.battery)
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        AppButton {
                            objectName: "bleScanButton"
                            text: ble.scanning ? qsTr("扫描中…") : qsTr("扫描设备")
                            enabled: !ble.scanning
                        }

                        AppButton {
                            objectName: "bleDisconnectButton"
                            text: qsTr("断开")
                            danger: true
                            enabled: ble.connected
                        }
                    }

                    Text {
                        width: parent.width
                        text: ble.status
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }

                    ListView {
                        id: bleDeviceList

                        objectName: "bleDeviceList"
                        width: parent.width
                        height: Math.min(contentHeight, 360)
                        visible: ble.devices.length > 0
                        clip: true
                        model: ble.devices
                        spacing: Metrics.spacing2xs

                        delegate: Rectangle {
                            width: bleDeviceList.width
                            height: 32
                            radius: Metrics.radiusXs
                            color: Theme.surfaceAlt
                            border.width: Metrics.borderWidth
                            border.color: Theme.divider

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: Metrics.spacingMd
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.name + "   " + modelData.address
                                color: Theme.textSecondary
                                font.pixelSize: Typography.fontBody
                            }

                            MouseArea {
                                objectName: "bleDeviceClick"
                                anchors.fill: parent
                                property string address: modelData.address
                            }
                        }
                    }

                    RowLayout {
                        width: parent.width
                        spacing: Metrics.spacingMd

                        Text {
                            text: qsTr("强度")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        Text {
                            text: "A " + ble.strengthA
                            color: Theme.accent
                            font.pixelSize: Typography.fontBody
                            font.bold: true
                        }

                        AppButton {
                            objectName: "bleStrengthADecrease"
                            text: "A－"
                            enabled: ble.connected
                        }

                        AppButton {
                            objectName: "bleStrengthAIncrease"
                            text: "A＋"
                            enabled: ble.connected
                        }

                        Text {
                            text: "B " + ble.strengthB
                            color: Theme.accent
                            font.pixelSize: Typography.fontBody
                            font.bold: true
                        }

                        AppButton {
                            objectName: "bleStrengthBDecrease"
                            text: "B－"
                            enabled: ble.connected
                        }

                        AppButton {
                            objectName: "bleStrengthBIncrease"
                            text: "B＋"
                            enabled: ble.connected
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            // 日志
            GlassCard {
                Layout.fillWidth: true
                title: qsTr("日志")

                RowLayout {
                    width: parent.width
                    spacing: Metrics.spacingMd

                    Text {
                        text: qsTr("级别")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                    }

                    AppComboBox {
                        id: logLevelCombo

                        objectName: "logLevelCombo"
                        Layout.preferredWidth: ComponentStyle.inputMinWidth
                        model: ["DEBUG", "INFO", "WARN", "ERROR"]
                        currentIndex: logBridge.levelFilter
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    AppButton {
                        objectName: "logExportButton"
                        text: qsTr("导出日志")
                    }

                    AppButton {
                        objectName: "logClearButton"
                        text: qsTr("清空")
                    }
                }

                // 贴底跟随：stickToBottom 由 C++ 侧（UiConnector）按用户滚动结果维护，
                // 绑定只在需要跟随时生效，用户上滚回看历史日志时不再被新日志拽回底部。
                ListView {
                    id: logList

                    objectName: "configLogList"
                    width: parent.width
                    height: ComponentStyle.logListHeight
                    clip: true
                    model: logBridge.messages
                    spacing: 0
                    property bool stickToBottom: true

                    delegate: Rectangle {
                        width: logList.width
                        height: ComponentStyle.logRowHeight
                        color: Theme.transparentFill

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            text: modelData.text
                            color: modelData.level >= 3 ? Theme.danger : (modelData.level === 2 ? Theme.warning : Theme.textSecondary)
                            font.family: Typography.fontMonoFamily
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }
                    }

                    // 无信号处理器：新日志到达时内容高度变化，绑定自动滚到底部；
                    // stickToBottom 为 false 时绑定不生效（restoreMode 不还原），用户停留原处
                    Binding {
                        target: logList
                        property: "contentY"
                        value: Math.max(0, logList.contentHeight - logList.height)
                        when: logList.stickToBottom
                        restoreMode: Binding.RestoreNone
                    }
                }

                Text {
                    width: parent.width
                    text: qsTr("导出目录：%1").arg(logBridge.exportDir)
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                    elide: Text.ElideMiddle
                }
            }

            Item {
                Layout.preferredHeight: Metrics.pageMargin
            }
        }
    }

    // 预设主题对话框
    Dialog {
        id: themePresetDialog

        objectName: "themePresetDialog"
        title: qsTr("选择主题")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 560
        height: 460
        padding: Metrics.spacingMd

        background: Rectangle {
            radius: Metrics.radiusMd
            color: Theme.surface
            border.width: Metrics.borderWidth
            border.color: Theme.border
        }

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
                    anchors.margins: Metrics.spacing2xs
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
                            font.pixelSize: Typography.fontBody
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

    // 自定义主题对话框
    Dialog {
        id: customThemeDialog

        objectName: "customThemeDialog"
        title: qsTr("自定义主题")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 420
        height: 260
        padding: Metrics.spacingLg

        background: Rectangle {
            radius: Metrics.radiusMd
            color: Theme.surface
            border.width: Metrics.borderWidth
            border.color: Theme.border
        }

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
                    font.pixelSize: Typography.fontBody
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
                    font.pixelSize: Typography.fontBody
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
        selectedColor: customThemeDialog.customPrimary
    }

    ColorDialog {
        id: secondaryColorDialog
        objectName: "secondaryColorDialog"
        title: qsTr("选择副色")
        selectedColor: customThemeDialog.customSecondary
    }
}
