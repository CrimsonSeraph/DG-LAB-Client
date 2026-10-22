import QtQuick
import QtQuick.Controls

// 通用按钮：primary = 强调色，danger = 危险色，默认 = 表面色。
// 交互约定：QML 不写 onClicked，点击由 C++ 侧按 objectName 连接。
// 状态：hovered / down / enabled / activeFocus / visualFocus，底色与文字全部取 Theme 令牌。
Button {
    id: control

    property bool primary: false
    property bool danger: false

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
            if (control.danger) {
                return control.down ? Theme.dangerPressed : (control.hovered ? Theme.dangerHover : Theme.danger);
            }
            if (control.primary) {
                return control.down ? Theme.pressed : (control.hovered ? Theme.hover : Theme.accent);
            }
            return control.down ? Theme.surfacePressed : (control.hovered ? Theme.surfaceAlt : Theme.surface);
        }
        border.width: Metrics.borderWidth
        border.color: !control.enabled ? Theme.borderDisabled : ((control.primary || control.danger) ? Theme.transparentFill : Theme.border)

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
