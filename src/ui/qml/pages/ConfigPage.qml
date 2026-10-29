import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// 配置页：连接（中转表单 + 扫码配对 + 蓝牙直连）与主题，以及规则编辑器入口。
// 波形内容在波形设置页，日志内容在日志页，本页不再承载。
// 卡片语言与首页一致：层级只在卡片这一层，卡片内分组一律「小标题 + 留白」，
// 不画内层描边、不填内层底色，分组间距由 ComponentStyle.groupSpacing 承担。
// 交互约定：本文件不写任何信号处理器；交互控件由 C++（UiConnector）按 objectName 连接。
Item {
    id: page

    objectName: "configPage"

    // 蓝牙直连状态行文案：已连接显示设备名，未连接只显示「未连接」
    readonly property string bleStatusLine: ble.connected
        ? qsTr("已连接 %1").arg(ble.deviceName)
        : qsTr("未连接")

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

            // 连接：上排左列中转表单、右列扫码配对（右列固定宽度，不再独占整卡），蓝牙直连在扫码配对列下方
            GlassCard {
                id: connectCard

                objectName: "configConnectionCard"
                Layout.fillWidth: true
                title: qsTr("连接")

                ColumnLayout {
                    width: parent.width
                    spacing: ComponentStyle.groupSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: ComponentStyle.groupSpacing

                        // 左列：地址 / 端口 / 连接 / 状态行 / 错误行
                        ColumnLayout {
                            objectName: "configConnectionFormColumn"
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            // 顶对齐：右列二维码较高时表单仍从列首开始，不被行高居中
                            Layout.alignment: Qt.AlignTop
                            spacing: ComponentStyle.groupTitleGap

                            Text {
                                text: qsTr("中转连接")
                                color: Theme.textSecondary
                                font.pixelSize: Typography.fontBody
                                font.bold: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
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
                                    Layout.preferredWidth: ComponentStyle.configIpFieldWidth
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
                                    Layout.preferredWidth: ComponentStyle.configPortFieldWidth
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

                            // 网卡 IP 选择：按黑/白名单过滤后手动指定中转地址
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Metrics.spacingMd

                                AppButton {
                                    objectName: "configSelectIpButton"
                                    text: qsTr("选择网卡 IP")
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("自动选择会跳过 VPN 与虚拟网卡")
                                    color: Theme.textMuted
                                    font.pixelSize: Typography.fontBody
                                    elide: Text.ElideRight
                                }
                            }

                            // 状态行：按配对阶段显示
                            Text {
                                Layout.fillWidth: true
                                text: device.pairingStage === 2 ? qsTr("已配对")
                                    : (device.pairingStage === 1 ? qsTr("等待 APP 扫码配对") : qsTr("未启动"))
                                color: device.pairingStage === 2 ? Theme.success
                                    : (device.pairingStage === 1 ? Theme.warning : Theme.textMuted)
                                font.pixelSize: Typography.fontBody
                                elide: Text.ElideRight
                            }

                            // 错误行：有错误时单独占一行，不与状态行混排
                            Text {
                                Layout.fillWidth: true
                                visible: device.lastError.length > 0
                                text: device.lastError
                                color: Theme.danger
                                font.pixelSize: Typography.fontBody
                                elide: Text.ElideRight
                            }
                        }

                        // 右列：扫码配对（宽度令牌固定，列内二维码按边长令牌）
                        ColumnLayout {
                            objectName: "configQrColumn"
                            Layout.fillWidth: false
                            Layout.preferredWidth: ComponentStyle.configQrColumnWidth
                            spacing: ComponentStyle.groupTitleGap

                            Text {
                                text: qsTr("扫码配对")
                                color: Theme.textSecondary
                                font.pixelSize: Typography.fontBody
                                font.bold: true
                            }

                            Image {
                                objectName: "configQrImage"
                                Layout.preferredWidth: ComponentStyle.configQrSize
                                Layout.preferredHeight: ComponentStyle.configQrSize
                                visible: device.connected
                                source: device.qrImageUrl
                                sourceSize.width: ComponentStyle.configQrSize
                                sourceSize.height: ComponentStyle.configQrSize
                                fillMode: Image.PreserveAspectFit
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: !device.connected
                                text: qsTr("连接后显示配对二维码")
                                color: Theme.textMuted
                                font.pixelSize: Typography.fontBody
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                objectName: "configPairingUrlText"
                                Layout.fillWidth: true
                                visible: device.connected
                                text: device.pairingUrlText
                                color: Theme.textMuted
                                font.pixelSize: Typography.fontBody
                                elide: Text.ElideMiddle
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: device.connected
                                text: device.v3Paired ? qsTr("APP 已配对")
                                    : (device.v4Attached ? qsTr("V4 已接入，等待设备上报") : qsTr("等待 APP 扫码配对"))
                                color: (device.v3Paired || device.v4Attached) ? Theme.accent : Theme.textMuted
                                font.pixelSize: Typography.fontBody
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    // 蓝牙直连（郊狼 V3）：位于扫码配对列下方，占整卡宽度
                    ColumnLayout {
                        objectName: "configBleColumn"
                        Layout.fillWidth: true
                        spacing: ComponentStyle.groupTitleGap

                        Text {
                            text: qsTr("蓝牙直连（郊狼 V3）")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                            font.bold: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Metrics.spacingMd

                            Text {
                                objectName: "configBleStatusText"
                                text: page.bleStatusLine
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

                        // 与状态行文案相同的状态不再重复显示，扫描中 / 错误 / 断开等有价值的状态保留
                        Text {
                            objectName: "configBleStatusDetail"
                            Layout.fillWidth: true
                            visible: ble.status.length > 0 && ble.status !== page.bleStatusLine
                            text: ble.status
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }

                        // 设备行常驻细边框与圆角，悬停改底色与描边并在热区上给出手型光标
                        ListView {
                            id: bleDeviceList

                            objectName: "bleDeviceList"
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(contentHeight, ComponentStyle.panelBlockMaxHeight)
                            visible: ble.devices.length > 0
                            clip: true
                            model: ble.devices
                            spacing: Metrics.spacing2xs

                            // 行尾徽标是驱动支持程度三态：已支持（郊狼）/ 实验性（驱动尚未实现）/ 暂不支持，
                            // 只有前两态可点选，暂不支持的行热区禁用
                            delegate: Rectangle {
                                width: bleDeviceList.width
                                height: ComponentStyle.inputHeight
                                radius: Metrics.radiusSm
                                color: deviceRowArea.containsMouse ? Theme.surfaceAlt : Theme.surface
                                border.width: Metrics.borderWidth
                                border.color: deviceRowArea.containsMouse ? Theme.accent : Theme.divider

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Metrics.spacingMd
                                    anchors.rightMargin: Metrics.spacingMd
                                    spacing: Metrics.spacingSm

                                    Text {
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 0
                                        text: modelData.name + "   " + modelData.address
                                            + (modelData.hint.length > 0 ? "   " + modelData.hint : "")
                                        color: modelData.connectable ? Theme.textSecondary : Theme.textMuted
                                        font.pixelSize: Typography.fontBody
                                        elide: Text.ElideRight
                                    }

                                    StatusBadge {
                                        objectName: "bleDeviceSupportBadge"
                                        Layout.alignment: Qt.AlignVCenter
                                        text: modelData.supportText
                                        level: modelData.support === 2 ? 1 : (modelData.support === 1 ? 2 : 3)
                                    }
                                }

                                MouseArea {
                                    id: deviceRowArea

                                    objectName: "bleDeviceClick"
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    enabled: modelData.connectable
                                    cursorShape: modelData.connectable ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    property string address: modelData.address
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
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

                    // 主色：色块（描边取文字令牌，对卡片底色恒 ≥4.5:1，任意填充色都能看出边界）+ 标签，hex 作为次级文本
                    Rectangle {
                        Layout.preferredWidth: ComponentStyle.swatchMinWidth
                        Layout.preferredHeight: ComponentStyle.swatchHeight
                        radius: Metrics.radiusSm
                        color: Theme.primary
                        border.width: Metrics.borderWidth
                        border.color: Theme.textMuted

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }

                    Column {
                        spacing: Metrics.spacing2xs

                        Text {
                            text: qsTr("主色")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        Text {
                            text: Theme.primary
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: ComponentStyle.swatchMinWidth
                        Layout.preferredHeight: ComponentStyle.swatchHeight
                        radius: Metrics.radiusSm
                        color: Theme.secondary
                        border.width: Metrics.borderWidth
                        border.color: Theme.textMuted

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }

                    Column {
                        spacing: Metrics.spacing2xs

                        Text {
                            text: qsTr("副色")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        Text {
                            text: Theme.secondary
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                        }
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
                Layout.preferredHeight: Metrics.pageMargin
            }
        }
    }

    // 预设主题对话框
    AppDialog {
        id: themePresetDialog

        objectName: "themePresetDialog"
        title: qsTr("选择主题")
        // 理想尺寸，由 AppDialog 钳制到视口内，避免最小窗口下对话框被裁切
        preferredWidth: 560
        preferredHeight: 460

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
    AppDialog {
        id: customThemeDialog

        objectName: "customThemeDialog"
        title: qsTr("自定义主题")
        // 理想尺寸，由 AppDialog 钳制到视口内，避免最小窗口下对话框被裁切
        preferredWidth: 420
        preferredHeight: 260

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
