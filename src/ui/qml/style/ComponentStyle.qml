pragma Singleton
import QtQuick

// 组件专属度量：通道卡片 / 强度卡 / 波形预览 / 节点编辑器等。
QtObject {
    // 首页通道卡片
    readonly property int channelCardMinWidth: 320
    readonly property int strengthValueWidth: 96
    readonly property int channelHeaderHeight: 40

    // 波形预览
    readonly property int wavePreviewHeight: 72
    readonly property int wavePreviewMinWidth: 160
    readonly property int waveInset: 3
    readonly property int waveStrokeWidth: 2
    readonly property int waveFrequencyStrokeWidth: 1

    // 规则节点编辑器
    readonly property int graphGridSize: 40
    readonly property int nodeWidth: 168
    readonly property int nodeHeaderHeight: 26
    readonly property int nodePortSize: 12
    readonly property int nodePortSpacing: 18
    readonly property int edgeWidth: 2
    readonly property int paletteWidth: 210

    // 主题色块
    readonly property int swatchHeight: 28
    readonly property int swatchMinWidth: 64

    // 日志面板
    readonly property int logRowHeight: 18
    readonly property int logListHeight: 170

    // 模块卡片
    readonly property int moduleCardWidth: 232
    readonly property int moduleCardHeight: 118
    readonly property int panelBlockMaxHeight: 180

    // 规则图画布世界尺寸与端口命中范围
    readonly property int graphWorldWidth: 4000
    readonly property int graphWorldHeight: 3000
    readonly property int graphPortHitTolerance: 12

    // 对话框尺寸
    readonly property int dialogRuleEditorWidth: 1100
    readonly property int dialogRuleEditorHeight: 700
    readonly property int dialogWaveEditorWidth: 900
    readonly property int dialogWaveEditorHeight: 640
    readonly property int dialogWaveSelectWidth: 540
    readonly property int dialogWaveSelectHeight: 500
    readonly property int dialogSidePanelWidth: 250
    readonly property int dialogListRowHeight: 30

    // 输入控件
    readonly property int inputHeight: 30
    readonly property int inputMinWidth: 120
    readonly property int inputPadding: 8
    readonly property int spinIndicatorWidth: 22
    readonly property int spinContentPadding: 6
    readonly property int checkIndicatorSize: 16
}
