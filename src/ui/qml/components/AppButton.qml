import QtQuick
import QtQuick.Controls

// 通用按钮：primary = 强调色，danger = 危险色，默认 = 表面色。
// 交互约定：QML 不写 onClicked，点击由 C++ 侧按 objectName 连接。
Button {
    id: control

    property bool primary: false
    property bool danger: false

    implicitHeight: Metrics.buttonHeight
    leftPadding: Metrics.spacingLg
    rightPadding: Metrics.spacingLg

    background: Rectangle {
        radius: Metrics.radiusSm
        color: {
            if (control.danger) {
                return control.down ? Qt.darker(Theme.danger, 1.2) : (control.hovered ? Qt.lighter(Theme.danger, 1.1) : Theme.danger);
            }
            if (control.primary) {
                return control.down ? Theme.pressed : (control.hovered ? Theme.hover : Theme.accent);
            }
            return control.down ? Qt.darker(Theme.surfaceAlt, 1.08) : (control.hovered ? Theme.surfaceAlt : Theme.surface);
        }
        border.width: Metrics.borderWidth
        border.color: (control.primary || control.danger) ? "transparent" : Theme.border
    }

    contentItem: Text {
        text: control.text
        color: (control.primary || control.danger) ? Theme.accentText : Theme.textPrimary
        font.pixelSize: Typography.fontBody
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
