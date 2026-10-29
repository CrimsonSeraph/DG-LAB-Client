import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 设备管理页：蓝牙扫描、已连接链路、默认设备与按设备的通道上下限。
// 交互约定：本文件不写信号处理器；扫描 / 连接 / 断开 / 下拉 / 上下限全部由 C++（UiConnector）按 objectName 连接。
Item {
    id: page

    objectName: "devicePage"

    // C++ 写入：正在编辑上下限的设备 MAC；该通道是否仍在用通道回退值（回退时标「默认」）
    property string limitsAddress: ""
    property bool limitsFallbackA: true
    property bool limitsFallbackB: true
    // C++ 写入：上下限编辑目标（在线输出设备 + 已记忆设备，离线项名称带「离线」）
    property var limitTargets: []

    ScrollView {
        id: scrollView

        objectName: "deviceScrollView"
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
                title: qsTr("设备管理")
                subtitle: ble.scanning
                    ? qsTr("正在扫描附近的设备…")
                    : (ble.connected ? qsTr("默认设备 %1").arg(ble.deviceName) : qsTr("无默认设备"))

                AppButton {
                    objectName: "deviceScanButton"
                    text: ble.scanning ? qsTr("扫描中…") : qsTr("扫描设备")
                    enabled: !ble.scanning
                }
            }

            // 扫描：结果按 MAC 一行，行尾按钮按该 MAC 当前是否已连接给「连接」或「断开」
            GlassCard {
                objectName: "deviceScanCard"
                Layout.fillWidth: true
                title: qsTr("扫描")

                ColumnLayout {
                    width: parent.width
                    spacing: ComponentStyle.groupTitleGap

                    Text {
                        objectName: "deviceScanStatusText"
                        Layout.fillWidth: true
                        text: ble.status.length > 0 ? ble.status : qsTr("点击「扫描设备」开始发现附近的设备")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }

                    ListView {
                        id: scanList

                        objectName: "deviceScanList"
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(contentHeight, ComponentStyle.panelBlockMaxHeight)
                        visible: ble.devices.length > 0
                        clip: true
                        model: ble.devices
                        spacing: Metrics.spacing2xs

                        delegate: Rectangle {
                            width: scanList.width
                            height: scanRow.implicitHeight + Metrics.spacingMd * 2
                            radius: Metrics.radiusSm
                            color: Theme.surface
                            border.width: Metrics.borderWidth
                            border.color: Theme.divider

                            RowLayout {
                                id: scanRow

                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: Metrics.spacingMd
                                anchors.rightMargin: Metrics.spacingMd
                                spacing: Metrics.spacingMd

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 0
                                    spacing: 0

                                    Text {
                                        objectName: "deviceScanRowName"
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: modelData.connectable ? Theme.textPrimary : Theme.textMuted
                                        font.pixelSize: Typography.fontBody
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        objectName: "deviceScanRowDetail"
                                        Layout.fillWidth: true
                                        text: modelData.address
                                            + (modelData.model.length > 0 ? " · " + qsTr("型号 %1").arg(modelData.model) : "")
                                            + (modelData.hint.length > 0 ? " · " + modelData.hint : "")
                                        color: Theme.textMuted
                                        font.pixelSize: Typography.fontBody
                                        elide: Text.ElideRight
                                    }
                                }

                                StatusBadge {
                                    objectName: "deviceScanRowSupport"
                                    Layout.alignment: Qt.AlignVCenter
                                    text: modelData.supportText
                                    level: modelData.support === 2 ? 1 : (modelData.support === 1 ? 2 : 3)
                                }

                                AppButton {
                                    objectName: "deviceScanActionButton"
                                    Layout.alignment: Qt.AlignVCenter
                                    property string address: modelData.address
                                    text: modelData.connected ? qsTr("断开") : qsTr("连接")
                                    danger: modelData.connected
                                    enabled: modelData.connected || modelData.connectable
                                }
                            }
                        }
                    }

                    Text {
                        objectName: "deviceScanEmpty"
                        Layout.fillWidth: true
                        visible: !ble.scanning && ble.devices.length === 0
                        text: qsTr("未发现设备：点击「扫描设备」重新扫描")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }
                }
            }

            // 已连接设备：数据来自 ble.connectedLinks（MAC / 型号 / 电量 / 连接序号），每行可单独断开
            GlassCard {
                objectName: "deviceLinksCard"
                Layout.fillWidth: true
                title: qsTr("已连接设备")

                ColumnLayout {
                    width: parent.width
                    spacing: ComponentStyle.groupTitleGap

                    Text {
                        objectName: "deviceLinkCount"
                        Layout.fillWidth: true
                        text: qsTr("共 %1 条蓝牙链路").arg(ble.connectedLinks.length)
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }

                    ListView {
                        id: linkList

                        objectName: "deviceLinkList"
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(contentHeight, ComponentStyle.panelBlockMaxHeight)
                        visible: ble.connectedLinks.length > 0
                        clip: true
                        model: ble.connectedLinks
                        spacing: Metrics.spacing2xs

                        delegate: Rectangle {
                            width: linkList.width
                            height: linkRow.implicitHeight + Metrics.spacingMd * 2
                            radius: Metrics.radiusSm
                            color: Theme.surface
                            border.width: Metrics.borderWidth
                            border.color: Theme.divider

                            RowLayout {
                                id: linkRow

                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: Metrics.spacingMd
                                anchors.rightMargin: Metrics.spacingMd
                                spacing: Metrics.spacingMd

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 0
                                    spacing: 0

                                    Text {
                                        objectName: "deviceLinkRowTitle"
                                        Layout.fillWidth: true
                                        text: (modelData.label.length > 0 ? modelData.label : modelData.type) + " · " + modelData.address
                                        color: Theme.textPrimary
                                        font.pixelSize: Typography.fontBody
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        objectName: "deviceLinkRowDetail"
                                        Layout.fillWidth: true
                                        // 输入设备（爪印 / 灵猫）额外显示实例数值模块名，输出链路没有该字段
                                        text: qsTr("型号 %1 · 电量 %2 · 连接序号 %3")
                                            .arg(modelData.model.length > 0 ? modelData.model : "—")
                                            .arg(modelData.battery >= 0 ? modelData.battery + "%" : "—")
                                            .arg(modelData.order)
                                            + (modelData.moduleName !== undefined && modelData.moduleName.length > 0
                                                ? qsTr(" · 模块 %1").arg(modelData.moduleName)
                                                : "")
                                        color: Theme.textMuted
                                        font.pixelSize: Typography.fontBody
                                        elide: Text.ElideRight
                                    }
                                }

                                AppButton {
                                    objectName: "deviceLinkDisconnectButton"
                                    Layout.alignment: Qt.AlignVCenter
                                    property string address: modelData.address
                                    text: qsTr("断开")
                                    danger: true
                                }
                            }
                        }
                    }

                    Text {
                        objectName: "deviceLinkEmpty"
                        Layout.fillWidth: true
                        visible: ble.connectedLinks.length === 0
                        text: qsTr("暂无已连接的蓝牙设备")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }
                }
            }

            // 默认设备：只列在线输出设备，写 app.output.default_device；强度增减下发给默认设备
            GlassCard {
                objectName: "deviceDefaultCard"
                Layout.fillWidth: true
                title: qsTr("默认设备")

                ColumnLayout {
                    width: parent.width
                    spacing: ComponentStyle.groupTitleGap

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingMd

                        AppComboBox {
                            objectName: "deviceDefaultCombo"
                            Layout.fillWidth: true
                            visible: device.outputDevices.length > 0
                            model: device.outputDevices
                            textRole: "name"
                            currentIndex: device.defaultDeviceIndex
                        }

                        Text {
                            objectName: "deviceDefaultEmpty"
                            Layout.fillWidth: true
                            visible: device.outputDevices.length === 0
                            text: qsTr("无在线输出设备")
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }
                    }

                    Text {
                        objectName: "deviceDefaultRuleText"
                        Layout.fillWidth: true
                        text: qsTr("评选规则：用户指定优先，否则郊狼 > 负鼠 > 同类型先连接者")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
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
                            objectName: "deviceDefaultStrengthA"
                            text: "A " + ble.strengthA
                            color: Theme.accent
                            font.pixelSize: Typography.fontBody
                            font.bold: true
                        }

                        AppButton {
                            objectName: "deviceStrengthADecrease"
                            text: "A－"
                            enabled: ble.connected
                        }

                        AppButton {
                            objectName: "deviceStrengthAIncrease"
                            text: "A＋"
                            enabled: ble.connected
                        }

                        Text {
                            objectName: "deviceDefaultStrengthB"
                            text: "B " + ble.strengthB
                            color: Theme.accent
                            font.pixelSize: Typography.fontBody
                            font.bold: true
                        }

                        AppButton {
                            objectName: "deviceStrengthBDecrease"
                            text: "B－"
                            enabled: ble.connected
                        }

                        AppButton {
                            objectName: "deviceStrengthBIncrease"
                            text: "B＋"
                            enabled: ble.connected
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    Text {
                        objectName: "deviceDefaultStrengthHint"
                        Layout.fillWidth: true
                        text: qsTr("强度增减下发给默认设备（与迷你窗同一入口），未连接设备时点击不产生变化")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }
                }
            }

            // 通道上限与安全上限：先选设备，再分别编辑该设备 A / B 的上限与安全上限
            GlassCard {
                objectName: "deviceLimitCard"
                Layout.fillWidth: true
                title: qsTr("通道上限与安全上限")

                ColumnLayout {
                    width: parent.width
                    spacing: ComponentStyle.groupTitleGap

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingMd

                        Text {
                            text: qsTr("设备")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        // 目标列表含已记忆设备：设备断开后其上下限仍可查看与编辑（写入不要求在线）
                        AppComboBox {
                            objectName: "deviceLimitDeviceCombo"
                            Layout.fillWidth: true
                            visible: page.limitTargets.length > 0
                            model: page.limitTargets
                            textRole: "name"
                        }

                        Text {
                            objectName: "deviceLimitDeviceEmpty"
                            Layout.fillWidth: true
                            visible: page.limitTargets.length === 0
                            text: qsTr("无可选设备（无在线输出设备，也没有已记忆的设备）")
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }
                    }

                    Text {
                        objectName: "deviceLimitAddressText"
                        Layout.fillWidth: true
                        text: page.limitsAddress.length > 0
                            ? qsTr("正在编辑 %1 的通道上下限").arg(page.limitsAddress)
                            : qsTr("未选择设备，显示通道回退值")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingMd

                        Text {
                            text: qsTr("A 通道")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        Text {
                            text: qsTr("上限")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        AppSpinBox {
                            id: limitASpin

                            objectName: "deviceLimitA"
                            editable: true
                            from: 0
                            to: 200
                            stepSize: 1
                            enabled: page.limitsAddress.length > 0
                        }

                        Text {
                            text: qsTr("安全上限")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        AppSpinBox {
                            id: safetyASpin

                            objectName: "deviceSafetyA"
                            editable: true
                            from: 0
                            to: 200
                            stepSize: 1
                            enabled: page.limitsAddress.length > 0
                        }

                        StatusBadge {
                            objectName: "deviceLimitFallbackBadgeA"
                            Layout.alignment: Qt.AlignVCenter
                            visible: page.limitsFallbackA
                            text: qsTr("默认")
                            level: 0
                        }

                        Text {
                            objectName: "deviceEffectiveA"
                            text: qsTr("生效 %1").arg(Math.min(limitASpin.value, safetyASpin.value))
                            color: Theme.accent
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingMd

                        Text {
                            text: qsTr("B 通道")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        Text {
                            text: qsTr("上限")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        AppSpinBox {
                            id: limitBSpin

                            objectName: "deviceLimitB"
                            editable: true
                            from: 0
                            to: 200
                            stepSize: 1
                            enabled: page.limitsAddress.length > 0
                        }

                        Text {
                            text: qsTr("安全上限")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        AppSpinBox {
                            id: safetyBSpin

                            objectName: "deviceSafetyB"
                            editable: true
                            from: 0
                            to: 200
                            stepSize: 1
                            enabled: page.limitsAddress.length > 0
                        }

                        StatusBadge {
                            objectName: "deviceLimitFallbackBadgeB"
                            Layout.alignment: Qt.AlignVCenter
                            visible: page.limitsFallbackB
                            text: qsTr("默认")
                            level: 0
                        }

                        Text {
                            objectName: "deviceEffectiveB"
                            text: qsTr("生效 %1").arg(Math.min(limitBSpin.value, safetyBSpin.value))
                            color: Theme.accent
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    Text {
                        objectName: "deviceLimitHint"
                        Layout.fillWidth: true
                        text: qsTr("生效值取 min(上限, 安全上限)；未设置过的键沿用 app.channels 通道值与内置默认 200 / 150")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }
                }
            }

            Item {
                Layout.preferredHeight: Metrics.pageMargin
            }
        }
    }
}
