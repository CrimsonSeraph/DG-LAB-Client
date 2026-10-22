import QtQuick
import QtQuick.Controls

// 主题化多行输入框：供规则编辑器表达式等场景使用，令牌与 AppTextField 保持一致。
// 交互约定：QML 不写信号处理器，编辑完成由 C++（UiConnector）按 objectName 读取 text。
// 状态：enabled / readOnly / activeFocus / visualFocus 各有独立视觉，只读不等于禁用。
TextArea {
    id: control

    color: !control.enabled ? Theme.textDisabled : (control.readOnly ? Theme.textSecondary : Theme.textPrimary)
    placeholderTextColor: !control.enabled ? Theme.textDisabled : Theme.textMuted
    selectionColor: Theme.selectionBg
    selectedTextColor: !control.enabled ? Theme.textDisabled : Theme.textPrimary

    background: Rectangle {
        implicitWidth: ComponentStyle.inputMinWidth
        implicitHeight: ComponentStyle.inputMinWidth
        radius: Metrics.radiusSm
        color: !control.enabled ? Theme.surfaceDisabled : Theme.surfaceAlt
        border.width: (control.enabled && (control.activeFocus || control.visualFocus)) ? Metrics.focusRingWidth : Metrics.borderWidth
        border.color: {
            if (!control.enabled) {
                return Theme.borderDisabled;
            }
            return (control.activeFocus || control.visualFocus) ? Theme.focusRing : Theme.border;
        }
    }
}
