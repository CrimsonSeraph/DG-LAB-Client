import QtQuick
import QtQuick.Controls

// 主题化多行输入框：供规则编辑器表达式等场景使用，令牌与 AppTextField 保持一致。
// 交互约定：QML 不写信号处理器，编辑完成由 C++（UiConnector）按 objectName 读取 text。
TextArea {
    id: control

    color: Theme.textPrimary
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.selectionBg
    selectedTextColor: Theme.textPrimary

    background: Rectangle {
        implicitWidth: ComponentStyle.inputMinWidth
        implicitHeight: ComponentStyle.inputMinWidth
        radius: Metrics.radiusSm
        color: Theme.surfaceAlt
        border.width: control.activeFocus ? 2 : Metrics.borderWidth
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}
