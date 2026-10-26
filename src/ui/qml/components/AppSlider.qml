import QtQuick
import QtQuick.Controls

// 滑块：细轨道 + 强调色已选段 + 圆形强调色手柄，showValue 打开时右侧显示 valueText。
// 用法：AppSlider { from: 0; to: 100; value: 30; step: 1; showValue: true }

Item {
    id: root

    property alias value: slider.value
    property alias from: slider.from
    property alias to: slider.to
    property real step: 1
    property bool showValue: false
    property string valueText: Math.round(slider.value).toString()

    readonly property bool pressed: slider.pressed
    readonly property bool hovered: slider.hovered

    readonly property int trackThickness: 4
    readonly property int handleSize: ComponentStyle.checkIndicatorSize
    readonly property int valueLabelWidth: 40

    signal moved()

    implicitWidth: 220
    implicitHeight: ComponentStyle.swatchHeight
    opacity: root.enabled ? 1.0 : Metrics.disabledOpacity

    Slider {
        id: slider

        anchors.left: parent.left
        anchors.right: root.showValue ? valueLabel.left : parent.right
        anchors.rightMargin: root.showValue ? Metrics.spacingMd : 0
        anchors.verticalCenter: parent.verticalCenter
        height: root.height
        from: 0
        to: 100
        stepSize: root.step > 0 ? root.step : 0
        opacity: root.enabled ? 1.0 : Metrics.disabledOpacity

        onMoved: root.moved()

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + (slider.availableHeight - height) / 2
            width: slider.availableWidth
            height: root.trackThickness
            radius: height / 2
            color: {
                if (!slider.enabled) {
                    return Theme.surfaceDisabled;
                }
                return (slider.hovered || slider.pressed) ? Theme.surfacePressed : Theme.surfaceAlt;
            }
            border.width: Metrics.borderWidth
            border.color: slider.enabled ? Theme.border : Theme.borderDisabled

            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: parent.radius
                color: {
                    if (!slider.enabled) {
                        return Theme.textDisabled;
                    }
                    return slider.pressed ? Theme.pressed : Theme.accent;
                }
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + (slider.availableHeight - height) / 2
            width: root.handleSize
            height: root.handleSize
            radius: height / 2
            color: {
                if (!slider.enabled) {
                    return Theme.surfaceDisabled;
                }
                if (slider.pressed) {
                    return Theme.pressed;
                }
                return slider.hovered ? Theme.hover : Theme.accent;
            }
        }
    }

    Text {
        id: valueLabel

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: root.showValue ? root.valueLabelWidth : 0
        visible: root.showValue
        text: root.valueText
        color: root.enabled ? Theme.textSecondary : Theme.textDisabled
        font.pixelSize: Typography.fontBody
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
