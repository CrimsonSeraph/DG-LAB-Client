import QtQuick
import QtQuick.Controls

// 通用按钮：primary = 强调色，danger = 危险色，默认 = 表面色。
// outline = 描边态：底色透明、只留语义色描边与文字，用于「运行中」这类需要危险色
// 但不应被整条实心填充抢走视觉的位置；默认 false，既有调用点视觉不变。
// 交互约定：QML 不写 onClicked，点击由 C++ 侧按 objectName 连接。
// 状态：hovered / down / enabled / activeFocus / visualFocus，底色与文字全部取 Theme 令牌。
Button {
    id: control

    property bool primary: false
    property bool danger: false
    property bool outline: false

    implicitHeight: Metrics.buttonHeight
    leftPadding: Metrics.spacingLg
    rightPadding: Metrics.spacingLg
    opacity: control.enabled ? 1.0 : Metrics.disabledOpacity

    background: Rectangle {
        radius: Metrics.radiusSm
        color: {
            if (!control.enabled) {
                return Theme.surfaceDisabled;
            }
            if (control.outline) {
                // 悬停/按下都停在 surfaceAlt：surfacePressed 会把语义色文字压到 4.5:1 以下
                // （实测最低 3.81:1），按下态改由加粗描边表达
                return (control.hovered || control.down) ? Theme.surfaceAlt : Theme.transparentFill;
            }
            if (control.danger) {
                return control.down ? Theme.dangerPressed : (control.hovered ? Theme.dangerHover : Theme.danger);
            }
            if (control.primary) {
                return control.down ? Theme.pressed : (control.hovered ? Theme.hover : Theme.accent);
            }
            return control.down ? Theme.surfacePressed : (control.hovered ? Theme.surfaceAlt : Theme.surface);
        }
        border.width: (control.outline && control.down) ? Metrics.focusRingWidth : Metrics.borderWidth
        border.color: {
            if (!control.enabled) {
                return Theme.borderDisabled;
            }
            if (control.outline) {
                return control.danger ? Theme.danger : Theme.border;
            }
            return (control.primary || control.danger) ? Theme.transparentFill : Theme.border;
        }

        // 焦点环：键盘/程序聚焦时向外扩一圈
        Rectangle {
            anchors.fill: parent
            anchors.margins: -Metrics.focusRingOffset
            radius: parent.radius + Metrics.focusRingOffset
            color: Theme.transparentFill
            border.width: Metrics.focusRingWidth
            border.color: Theme.focusRing
            visible: control.enabled && (control.activeFocus || control.visualFocus)
        }
    }

    contentItem: Text {
        text: control.text
        color: {
            if (!control.enabled) {
                return Theme.textDisabled;
            }
            if (control.outline) {
                // 描边态文字落在 surface / surfaceAlt 上：danger 用 dangerOnAlt（该令牌按
                // surfaceAlt 保证 ≥4.5:1，surface 更浅/更暗总是更安全），其余用本身已双背景达标的 accent
                return control.danger ? Theme.dangerOnAlt : (control.primary ? Theme.accent : Theme.textPrimary);
            }
            if (control.danger) {
                return Theme.dangerText;
            }
            if (control.primary) {
                return Theme.accentText;
            }
            return Theme.textPrimary;
        }
        font.pixelSize: Typography.fontBody
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
