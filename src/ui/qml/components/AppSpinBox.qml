import QtQuick
import QtQuick.Controls

// 主题化 SpinBox：内容区 / 上下指示器 / 背景全部取自 Theme 令牌。
// 交互约定：QML 不写信号处理器，数值变化由 C++（UiConnector）按 objectName 连接。
// 状态：enabled / activeFocus / visualFocus / 指示器 pressed 全部取令牌。
SpinBox {
    id: control

    // 通道卡等场景用「键入 + 外侧 ± 按钮」，不需要内置上下箭头：
    // 置 false 时指示器宽度收成 0 并隐藏，原本留给箭头的横向空间回到数值区。
    property bool showIndicator: true

    contentItem: TextInput {
        z: 2
        text: control.displayText
        clip: width < implicitWidth
        padding: ComponentStyle.spinContentPadding

        font: control.font
        color: control.enabled ? Theme.textPrimary : Theme.textDisabled
        selectionColor: Theme.selectionBg
        selectedTextColor: control.enabled ? Theme.textPrimary : Theme.textDisabled
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter

        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
    }

    up.indicator: Rectangle {
        x: control.mirrored ? 0 : control.width - width
        height: control.height
        implicitWidth: control.showIndicator ? ComponentStyle.spinIndicatorWidth : 0
        implicitHeight: control.showIndicator ? ComponentStyle.spinIndicatorWidth : 0
        visible: control.showIndicator
        radius: Metrics.radiusSm
        color: !control.enabled ? Theme.surfaceDisabled : (control.up.pressed ? Theme.pressed : Theme.surface)
        border.width: Metrics.borderWidth
        border.color: control.enabled ? Theme.border : Theme.borderDisabled

        Text {
            anchors.centerIn: parent
            text: "+"
            color: control.enabled ? Theme.textPrimary : Theme.textDisabled
            font.pixelSize: Typography.fontBody
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? parent.width - width : 0
        height: control.height
        implicitWidth: control.showIndicator ? ComponentStyle.spinIndicatorWidth : 0
        implicitHeight: control.showIndicator ? ComponentStyle.spinIndicatorWidth : 0
        visible: control.showIndicator
        radius: Metrics.radiusSm
        color: !control.enabled ? Theme.surfaceDisabled : (control.down.pressed ? Theme.pressed : Theme.surface)
        border.width: Metrics.borderWidth
        border.color: control.enabled ? Theme.border : Theme.borderDisabled

        Text {
            anchors.centerIn: parent
            text: "−"
            color: control.enabled ? Theme.textPrimary : Theme.textDisabled
            font.pixelSize: Typography.fontBody
        }
    }

    background: Rectangle {
        implicitWidth: ComponentStyle.inputMinWidth
        implicitHeight: ComponentStyle.inputHeight
        radius: Metrics.radiusSm
        color: control.enabled ? Theme.surfaceAlt : Theme.surfaceDisabled
        border.width: (control.enabled && (control.activeFocus || control.visualFocus)) ? Metrics.focusRingWidth : Metrics.borderWidth
        border.color: {
            if (!control.enabled) {
                return Theme.borderDisabled;
            }
            return (control.activeFocus || control.visualFocus) ? Theme.focusRing : Theme.border;
        }
    }
}
