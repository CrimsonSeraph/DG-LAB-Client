import QtQuick
import QtQuick.Controls

// 带状态徽标的按钮：徽标圆点按 statusLevel 取语义色，clicked 直接透传。
// 用法：AppStatusButton { text: "运行中"; statusLevel: 1 }

Button {
    id: control

    property int statusLevel: 0

    readonly property int badgeSize: 6
    readonly property color statusColor: {
        if (control.statusLevel === 1) {
            return Theme.success;
        }
        if (control.statusLevel === 2) {
            return Theme.warning;
        }
        if (control.statusLevel === 3) {
            return Theme.danger;
        }
        return Theme.textMuted;
    }

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
            return control.down ? Theme.surfacePressed : (control.hovered ? Theme.surfaceAlt : Theme.surface);
        }
        border.width: Metrics.borderWidth
        border.color: {
            if (!control.enabled) {
                return Theme.borderDisabled;
            }
            return (control.hovered || control.activeFocus || control.visualFocus) ? Theme.accent : Theme.border;
        }

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

    contentItem: Item {
        id: contentRoot

        readonly property real groupWidth: control.badgeSize + Metrics.spacingSm + contentLabel.implicitWidth

        Rectangle {
            id: badgeDot

            x: Math.max(0, (contentRoot.width - contentRoot.groupWidth) / 2)
            anchors.verticalCenter: parent.verticalCenter
            width: control.badgeSize
            height: control.badgeSize
            radius: width / 2
            color: control.enabled ? control.statusColor : Theme.textDisabled
        }

        Text {
            id: contentLabel

            x: badgeDot.x + badgeDot.width + Metrics.spacingSm
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, contentRoot.width - x)
            text: control.text
            color: control.enabled ? Theme.textPrimary : Theme.textDisabled
            font.pixelSize: Typography.fontBody
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
}
