import QtQuick
import QtQuick.Controls

// 主题化菜单：底色、描边、圆角与内边距取自 Theme / Metrics；项的文字色、高亮色、按下色与
// 分隔线经 palette 下传给声明式 MenuItem 与 MenuSeparator（控件从自身 palette 取色），
// 因此调用方按普通 Menu 写 MenuItem 即可跟随主题，不需要逐项改样式。
Menu {
    id: control

    padding: Metrics.spacingXs

    // 逐色组写而不是写 palette.windowText 这种全色组写法：全色组写法会把禁用组一起刷成同一色，
    // 禁用项就看不出已禁用
    palette.active.windowText: Theme.textPrimary
    palette.inactive.windowText: Theme.textPrimary
    palette.disabled.windowText: Theme.textDisabled
    palette.light: Theme.selectionBg
    palette.midlight: Theme.surfacePressed
    palette.mid: Theme.divider

    background: Rectangle {
        implicitWidth: Metrics.navWidth
        radius: Metrics.radiusSm
        color: Theme.surface
        border.width: Metrics.borderWidth
        border.color: Theme.border
    }
}
