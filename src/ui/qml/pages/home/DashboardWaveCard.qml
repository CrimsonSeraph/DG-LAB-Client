import QtQuick
import QtQuick.Layouts

// 单通道波形卡：当前绑定波形的强度曲线 + 强度/上限，正在下发时显示帧序号。
// 曲线取 WaveBridge 的逐帧强度均值（每帧 4 个强度字节取均值，0~100），静态绘制；
// 强度与上限跟随设备回传实时刷新，滚动播放留给迷你窗。
// 一张卡只渲染一个通道：首页把它与同名通道卡放在同一行并排。
GlassCard {
    id: root

    // 卡片渲染的通道（"A" / "B"）：取数与 objectName 都按它派生
    property string channel: "A"

    objectName: "dashboardWaveCard" + root.channel
    title: qsTr("通道 %1 波形").arg(root.channel)

    readonly property string waveName: {
        const name = (root.channel === "B") ? waveBridge.currentB : waveBridge.currentA;
        return (name === undefined || name === null) ? "" : String(name);
    }
    readonly property bool bound: root.waveName.length > 0
    readonly property var amplitudes: (root.channel === "B") ? waveBridge.amplitudeB : waveBridge.amplitudeA
    readonly property int strength: (root.channel === "B") ? home.strengthB : home.strengthA
    readonly property int limit: (root.channel === "B") ? home.limitB : home.limitA
    readonly property bool sending: (root.channel === "B") ? home.waveSendingB : home.waveSendingA
    readonly property int frame: (root.channel === "B") ? home.waveFrameB : home.waveFrameA
    readonly property int frames: (root.channel === "B") ? home.waveFramesB : home.waveFramesA

    Text {
        objectName: "dashboardWaveName" + root.channel
        width: parent.width
        visible: root.bound
        text: root.waveName
        color: Theme.textMuted
        font.pixelSize: Typography.fontBody
        elide: Text.ElideRight
    }

    Item {
        width: parent.width
        height: ComponentStyle.wavePreviewHeight

        AppSparkline {
            objectName: "dashboardWaveSparkline" + root.channel
            anchors.fill: parent
            visible: root.bound
            points: root.amplitudes
            maxValue: 100
            showAxisLabels: true
        }

        Text {
            objectName: "dashboardWaveEmpty" + root.channel
            anchors.centerIn: parent
            visible: !root.bound
            text: qsTr("未绑定波形")
            color: Theme.textMuted
            font.pixelSize: Typography.fontBody
        }
    }

    Text {
        objectName: "dashboardWaveSending" + root.channel
        width: parent.width
        visible: root.sending
        text: qsTr("正在下发 · 第 %1/%2 帧").arg(root.frame).arg(root.frames)
        color: Theme.success
        font.pixelSize: Typography.fontBody
        elide: Text.ElideRight
    }

    Text {
        objectName: "dashboardWaveStrength" + root.channel
        width: parent.width
        text: qsTr("强度 %1 / 上限 %2").arg(root.strength).arg(root.limit)
        color: Theme.textSecondary
        font.pixelSize: Typography.fontBody
        elide: Text.ElideRight
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
