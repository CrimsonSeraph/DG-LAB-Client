import QtQuick
import QtQuick.Controls

// 对话框基类：统一主题化底色与描边、模态遮罩、内边距、标题行与尺寸钳制。
// 子类只声明内容与被钳制前的理想尺寸；颜色、字号一律取 Theme / Metrics / Typography 令牌。
Dialog {
    id: dialog

    // 理想尺寸：实际宽高取它与视口之差中的较小值；preferredHeight 为 0 时高度由内容隐式高度决定
    property int preferredWidth: Metrics.navWidth * 2 + Metrics.spacing3xl * 2
    property int preferredHeight: 0

    modal: true
    anchors.centerIn: Overlay.overlay
    padding: Metrics.spacingMd
    width: Math.min(preferredWidth, Overlay.overlay.width - Metrics.spacingXl * 2)
    height: preferredHeight > 0
        ? Math.min(preferredHeight, Overlay.overlay.height - Metrics.spacingXl * 2)
        : Math.min(implicitHeight, Overlay.overlay.height - Metrics.spacingXl * 2)

    // 背景：Basic 样式默认底色不随主题变化，这里按令牌重绘
    background: Rectangle {
        radius: Metrics.radiusMd
        color: Theme.surface
        border.width: Metrics.borderWidth
        border.color: Theme.border
    }

    // 遮罩：压暗底层界面，不透明度取 Metrics.scrimOpacity（所有对话框共用同一令牌）
    Overlay.modal: Rectangle {
        color: Theme.windowBg
        opacity: Metrics.scrimOpacity
    }

    // 标题行：所有对话框共用同一字号与颜色，标题文本取 title 属性
    header: Text {
        topPadding: Metrics.spacingMd
        leftPadding: Metrics.spacingMd
        rightPadding: Metrics.spacingMd
        text: dialog.title
        color: Theme.textPrimary
        font.pixelSize: Typography.fontSubtitle
        font.bold: true
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    // 内容区：子类声明的对象挂在标题行以下的可用区域内
    contentItem: Item { }
}
