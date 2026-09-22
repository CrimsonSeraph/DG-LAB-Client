import QtQuick
import QtQuick.Controls

// 主题化 SpinBox：内容区 / 上下指示器 / 背景全部取自 Theme 令牌。
// 交互约定：QML 不写信号处理器，数值变化由 C++（UiConnector）按 objectName 连接。
SpinBox {
    id: control

    contentItem: TextInput {
        z: 2
        text: control.displayText
        clip: width < implicitWidth
        padding: ComponentStyle.spinContentPadding

        font: control.font
        color: control.enabled ? Theme.textPrimary : Theme.textMuted
        selectionColor: Theme.selectionBg
        selectedTextColor: Theme.textPrimary
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter

        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
    }

    up.indicator: Rectangle {
        x: control.mirrored ? 0 : control.width - width
        height: control.height
        implicitWidth: ComponentStyle.spinIndicatorWidth
        implicitHeight: ComponentStyle.spinIndicatorWidth
        radius: Metrics.radiusSm
        color: control.up.pressed ? Theme.pressed : Theme.surface
        border.width: Metrics.borderWidth
        border.color: Theme.border

        Text {
            anchors.centerIn: parent
            text: "+"
            color: control.enabled ? Theme.textPrimary : Theme.textMuted
            font.pixelSize: Typography.fontBody
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? parent.width - width : 0
        height: control.height
        implicitWidth: ComponentStyle.spinIndicatorWidth
        implicitHeight: ComponentStyle.spinIndicatorWidth
        radius: Metrics.radiusSm
        color: control.down.pressed ? Theme.pressed : Theme.surface
        border.width: Metrics.borderWidth
        border.color: Theme.border

        Text {
            anchors.centerIn: parent
            text: "−"
            color: control.enabled ? Theme.textPrimary : Theme.textMuted
            font.pixelSize: Typography.fontBody
        }
    }

    background: Rectangle {
        implicitWidth: ComponentStyle.inputMinWidth
        implicitHeight: ComponentStyle.inputHeight
        radius: Metrics.radiusSm
        color: Theme.surfaceAlt
        border.width: control.activeFocus ? 2 : Metrics.borderWidth
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}
