pragma Singleton
import QtQuick

// 组件专属度量：通道卡片 / 强度卡 / 波形预览 / 节点编辑器等。
QtObject {
    // ------------------------------------------------------------ 首页通道卡片
    readonly property int channelCardMinWidth: 320
    readonly property int strengthValueWidth: 96
    readonly property int channelHeaderHeight: 40

    // ---------------------------------------------------------------- 波形预览
    readonly property int wavePreviewHeight: 72
    readonly property int wavePreviewMinWidth: 160

    // ------------------------------------------------------------ 规则节点编辑器
    readonly property int graphGridSize: 20
    readonly property int nodeWidth: 168
    readonly property int nodeHeaderHeight: 26
    readonly property int nodePortSize: 12
    readonly property int nodePortSpacing: 18
    readonly property int edgeWidth: 2
    readonly property int paletteWidth: 210

    // ------------------------------------------------------------ 主题色块
    readonly property int swatchHeight: 28
    readonly property int swatchMinWidth: 64
}
