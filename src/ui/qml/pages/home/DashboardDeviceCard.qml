import QtQuick
import QtQuick.Layouts

// 设备状态卡：连接状态 + 中转地址 + A/B 通道强度与上限（只读）+ 通道启停。
GlassCard {
    id: root

    objectName: "dashboardDeviceCard"
    title: qsTr("设备状态")

    // 连接状态统一口径：蓝牙连接（0/1）+ APP 在线客户端数（来源 device.appClientCount）
    readonly property int bleCount: ble.connected ? 1 : 0
    readonly property int appCount: device.appClientCount
    readonly property int connectionCount: bleCount + appCount

    RowLayout {
        width: parent.width
        spacing: Metrics.spacingMd

        Rectangle {
            objectName: "dashboardDeviceDot"
            Layout.alignment: Qt.AlignTop
            implicitWidth: Metrics.spacingMd
            implicitHeight: Metrics.spacingMd
            radius: width / 2
            color: root.connectionCount > 0 ? Theme.success : Theme.textMuted
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            spacing: Metrics.spacing2xs

            Text {
                objectName: "dashboardDeviceName"
                Layout.fillWidth: true
                text: root.connectionCount > 0
                    ? qsTr("已连接设备 ×%1").arg(root.connectionCount)
                    : qsTr("未连接设备")
                color: Theme.textPrimary
                font.pixelSize: Typography.fontSubtitle
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                objectName: "dashboardDeviceState"
                Layout.fillWidth: true
                text: ble.connected ? qsTr("蓝牙已连接") : qsTr("蓝牙未连接")
                color: ble.connected ? Theme.success : Theme.textMuted
                font.pixelSize: Typography.fontBody
                elide: Text.ElideRight
            }

            Text {
                objectName: "dashboardDeviceAppState"
                Layout.fillWidth: true
                text: root.appCount > 0 ? qsTr("App 已连接 ×%1").arg(root.appCount) : qsTr("App 未连接")
                color: root.appCount > 0 ? Theme.success : Theme.textMuted
                font.pixelSize: Typography.fontBody
                elide: Text.ElideRight
            }

            Text {
                objectName: "dashboardDeviceBleName"
                Layout.fillWidth: true
                visible: ble.connected && ble.deviceName !== ""
                text: ble.deviceName
                color: Theme.textMuted
                font.pixelSize: Typography.fontBody
                elide: Text.ElideRight
            }
        }

        StatusBadge {
            objectName: "dashboardOutputBadge"
            Layout.alignment: Qt.AlignVCenter
            text: device.outputReady ? qsTr("输出可用") : qsTr("无可用输出")
            level: device.outputReady ? 1 : 2
        }
    }

    // 默认设备：只列在线输出设备（郊狼 / 负鼠等有输出能力的型号），
    // 切换后通道强度、波形与事件波形的下发目标一并改用该设备
    RowLayout {
        width: parent.width
        spacing: Metrics.spacingMd

        Text {
            text: qsTr("默认设备")
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
        }

        AppComboBox {
            objectName: "homeDefaultDeviceCombo"
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            visible: home.outputDevices.length > 0
            model: home.outputDevices
            textRole: "name"
            currentIndex: home.defaultDeviceIndex
        }

        Text {
            objectName: "homeDefaultDeviceEmpty"
            visible: home.outputDevices.length === 0
            text: qsTr("无在线输出设备")
            color: Theme.textMuted
            font.pixelSize: Typography.fontBody
        }
    }

    Text {
        objectName: "dashboardRelayState"
        width: parent.width
        text: qsTr("中转 %1 · %2:%3").arg(device.connected ? qsTr("已连接") : qsTr("未启动"))
            .arg(device.ip).arg(device.port)
        color: Theme.textMuted
        font.pixelSize: Typography.fontBody
        elide: Text.ElideRight
    }

    DashboardChannelRow {
        channel: "A"
        strength: home.strengthA
        limit: home.limitA
        running: home.runningA
        state: home.channelAState
    }

    DashboardChannelRow {
        channel: "B"
        strength: home.strengthB
        limit: home.limitB
        running: home.runningB
        state: home.channelBState
    }
}
