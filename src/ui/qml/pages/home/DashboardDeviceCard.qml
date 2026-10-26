import QtQuick
import QtQuick.Layouts

// 设备状态卡：连接状态 + 中转地址 + A/B 通道强度与上限（只读）+ 通道启停。
GlassCard {
    id: root

    objectName: "dashboardDeviceCard"
    title: qsTr("设备状态")

    RowLayout {
        width: parent.width
        spacing: Metrics.spacingMd

        Rectangle {
            objectName: "dashboardDeviceDot"
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: Metrics.spacingMd
            implicitHeight: Metrics.spacingMd
            radius: width / 2
            color: ble.connected ? Theme.success : Theme.textMuted
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            spacing: Metrics.spacing2xs

            Text {
                objectName: "dashboardDeviceName"
                Layout.fillWidth: true
                text: (ble.connected && ble.deviceName !== "") ? ble.deviceName : qsTr("未连接设备")
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
        }

        StatusBadge {
            objectName: "dashboardOutputBadge"
            Layout.alignment: Qt.AlignVCenter
            text: device.outputReady ? qsTr("输出可用") : qsTr("无可用输出")
            level: device.outputReady ? 1 : 2
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
