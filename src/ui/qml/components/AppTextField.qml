import QtQuick
import QtQuick.Controls

// 主题化单行输入框：文字 / 占位符 / 选区 / 背景全部取自 Theme 令牌，聚焦时描边转强调色。
// 交互约定：QML 不写信号处理器，编辑完成由 C++（UiConnector）按 objectName 读取 text。
TextField {
    id: control

    color: Theme.textPrimary
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.selectionBg
    selectedTextColor: Theme.textPrimary

    background: Rectangle {
        implicitWidth: ComponentStyle.inputMinWidth
        implicitHeight: ComponentStyle.inputHeight
        radius: Metrics.radiusSm
        color: Theme.surfaceAlt
        border.width: control.activeFocus ? 2 : Metrics.borderWidth
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}
