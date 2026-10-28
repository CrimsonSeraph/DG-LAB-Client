import QtQuick
import QtQuick.Layouts

// 通道波形卡：A / B 当前绑定波形的强度曲线并排只读预览。
// 曲线取 WaveBridge 的逐帧强度均值（每帧 4 个强度字节取均值，0~100），静态绘制；
// 强度与上限数字跟随设备回传实时刷新，正在下发时额外显示当前帧序号，滚动播放留给迷你窗批次。
GlassCard {
    id: root

    objectName: "dashboardWaveCard"
    title: qsTr("通道波形")

    // 两路预览共用同一套取数：通道名只在这里分叉，避免两段重复代码
    function waveName(channel) {
        const name = (channel === "B") ? waveBridge.currentB : waveBridge.currentA;
        return (name === undefined || name === null) ? "" : String(name);
    }

    function amplitudes(channel) {
        return (channel === "B") ? waveBridge.amplitudeB : waveBridge.amplitudeA;
    }

    function strengthOf(channel) {
        return (channel === "B") ? home.strengthB : home.strengthA;
    }

    function limitOf(channel) {
        return (channel === "B") ? home.limitB : home.limitA;
    }

    function sendingOf(channel) {
        return (channel === "B") ? home.waveSendingB : home.waveSendingA;
    }

    function frameOf(channel) {
        return (channel === "B") ? home.waveFrameB : home.waveFrameA;
    }

    function framesOf(channel) {
        return (channel === "B") ? home.waveFramesB : home.waveFramesA;
    }

    RowLayout {
        width: parent.width
        spacing: Metrics.spacingLg

        Repeater {
            model: ["A", "B"]

            delegate: ColumnLayout {
                id: channelColumn

                required property string modelData

                readonly property bool bound: root.waveName(channelColumn.modelData).length > 0

                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.alignment: Qt.AlignTop
                spacing: Metrics.spacingXs

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingSm

                    Text {
                        text: qsTr("通道 %1").arg(channelColumn.modelData)
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                        font.bold: true
                    }

                    Text {
                        objectName: "dashboardWaveName" + channelColumn.modelData
                        Layout.fillWidth: true
                        visible: channelColumn.bound
                        text: root.waveName(channelColumn.modelData)
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.preferredHeight: ComponentStyle.wavePreviewHeight

                    AppSparkline {
                        objectName: "dashboardWaveSparkline" + channelColumn.modelData
                        anchors.fill: parent
                        visible: channelColumn.bound
                        points: root.amplitudes(channelColumn.modelData)
                        maxValue: 100
                        showAxisLabels: true
                    }

                    Text {
                        objectName: "dashboardWaveEmpty" + channelColumn.modelData
                        anchors.centerIn: parent
                        visible: !channelColumn.bound
                        text: qsTr("未绑定波形")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                    }
                }

                Text {
                    objectName: "dashboardWaveSending" + channelColumn.modelData
                    Layout.fillWidth: true
                    visible: root.sendingOf(channelColumn.modelData)
                    text: qsTr("正在下发 · 第 %1/%2 帧").arg(root.frameOf(channelColumn.modelData)).arg(root.framesOf(channelColumn.modelData))
                    color: Theme.success
                    font.pixelSize: Typography.fontBody
                    elide: Text.ElideRight
                }

                Text {
                    objectName: "dashboardWaveStrength" + channelColumn.modelData
                    Layout.fillWidth: true
                    text: qsTr("强度 %1 / 上限 %2").arg(root.strengthOf(channelColumn.modelData)).arg(root.limitOf(channelColumn.modelData))
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                    elide: Text.ElideRight
                }
            }
        }
    }

    Text {
        objectName: "dashboardWaveHint"
        width: parent.width
        text: qsTr("静态预览 · 强度与下发帧随通道实时刷新，滚动播放在迷你窗提供")
        color: Theme.textMuted
        font.pixelSize: Typography.fontBody
        wrapMode: Text.WordWrap
    }
}
