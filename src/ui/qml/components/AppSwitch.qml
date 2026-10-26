import QtQuick
import QtQuick.Controls

// 开关：关闭态为灰轨，开启态为强调色轨 + 深色滑块，键盘可聚焦切换。
// 用法：AppSwitch { checked: true }

Switch {
    id: control

    readonly property int trackWidth: 40
    readonly property int trackHeight: 22
    readonly property int knobMargin: 3

    spacing: Metrics.spacingMd
    padding: Metrics.spacingXs
    opacity: control.enabled ? 1.0 : Metrics.disabledOpacity
    implicitHeight: Math.max(ComponentStyle.inputHeight, contentLabel.implicitHeight + control.topPadding + control.bottomPadding)

    indicator: Rectangle {
        id: track

        implicitWidth: control.trackWidth
        implicitHeight: control.trackHeight
        x: control.mirrored ? control.width - width - control.rightPadding : control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: height / 2
        color: {
            if (!control.enabled) {
                return Theme.surfaceDisabled;
            }
            if (control.checked) {
                return (control.hovered || control.down) ? Theme.hover : Theme.accent;
            }
            return (control.hovered || control.down) ? Theme.surfacePressed : Theme.surfaceAlt;
        }
        border.width: Metrics.borderWidth
        border.color: {
            if (!control.enabled) {
                return Theme.borderDisabled;
            }
            return control.checked ? Theme.accent : Theme.border;
        }

        Rectangle {
            id: knob

            readonly property int knobSize: track.height - control.knobMargin * 2

            y: control.knobMargin
            width: knobSize
            height: knobSize
            radius: knobSize / 2
            x: control.knobMargin + (track.width - knobSize - control.knobMargin * 2) * control.position
            color: {
                if (!control.enabled) {
                    return Theme.textDisabled;
                }
                return control.checked ? Theme.accentText : Theme.textMuted;
            }
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

    contentItem: Text {
        id: contentLabel

        leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0
        text: control.text
        color: control.enabled ? Theme.textPrimary : Theme.textDisabled
        font.pixelSize: Typography.fontBody
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
