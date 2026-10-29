pragma Singleton
import QtQuick

// 组件专属度量：通道卡片 / 强度卡 / 波形预览 / 节点编辑器等。
QtObject {
    // 卡片内分组：层级只由留白承担——分组用小标题 + 间距，不再画内层描边或填内层底色
    readonly property int groupTitleGap: 6
    readonly property int groupSpacing: 18
    readonly property int statusBadgeHeight: 22

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
    // 网格线透明度：横线与纵线共用同一参数，避免两处各写一个近似值
    readonly property real graphGridOpacity: 0.4
    readonly property int nodeWidth: 168
    readonly property int nodeHeaderHeight: 26
    readonly property int nodePortSize: 12
    readonly property int nodePortSpacing: 18
    readonly property int edgeWidth: 2
    readonly property int paletteWidth: 210

    // 主题色块
    readonly property int swatchHeight: 28
    readonly property int swatchMinWidth: 64

    // 日志面板：卡片高度由令牌给定，列表吃满卡片剩余高度（下限见 logListMinHeight）
    readonly property int logRowHeight: 18
    readonly property int logCardHeight: 360
    readonly property int logListMinHeight: 200

    // 配置页连接卡：右列二维码固定宽度（二维码边长 + 列内余量）
    readonly property int configQrSize: 200
    readonly property int configQrColumnWidth: 216
    readonly property int configIpFieldWidth: 180
    readonly property int configPortFieldWidth: 100

    // 模块卡片
    readonly property int moduleCardWidth: 232
    readonly property int moduleCardHeight: 118
    readonly property int panelBlockMaxHeight: 180

    // 规则图画布世界尺寸与端口命中范围
    readonly property int graphWorldWidth: 4000
    readonly property int graphWorldHeight: 3000
    // 端口命中容差是屏幕像素：用法为 graphPortHitTolerance / zoom
    readonly property int graphPortHitTolerance: 12
    // 连线曲线的控制点水平偏移（世界单位），绘制与命中测试共用
    readonly property int graphEdgeCurveOffset: 60
    // 缩放上下限，滚轮缩放与适配视图共用
    readonly property real graphZoomMin: 0.4
    readonly property real graphZoomMax: 2.0

    // 对话框尺寸
    readonly property int dialogRuleEditorWidth: 1100
    readonly property int dialogRuleEditorHeight: 700
    // 规则编辑器的初始尺寸之外，另给最小尺寸：三栏（左栏 210 + 画布 300 + 检查器 250）与底部按钮行的下限
    readonly property int dialogRuleEditorMinWidth: 900
    readonly property int dialogRuleEditorMinHeight: 600
    // 自绘调整手柄的热区边长（右下角）
    readonly property int dialogResizeHandleSize: 20
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
