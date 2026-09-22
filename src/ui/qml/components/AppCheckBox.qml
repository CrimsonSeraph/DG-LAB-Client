import QtQuick
import QtQuick.Controls

// 主题化勾选框：指示器与文本颜色取自 Theme 令牌，选中态使用强调色。
// 交互约定：QML 不写信号处理器，勾选状态由 C++（UiConnector）按 objectName 连接。
CheckBox {
    id: control

    spacing: Metrics.spacingSm
    padding: Metrics.spacingXs

    indicator: Rectangle {
        implicitWidth: ComponentStyle.checkIndicatorSize
        implicitHeight: ComponentStyle.checkIndicatorSize
        x: control.mirrored ? control.width - width - control.rightPadding : control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: Metrics.radiusXs
        color: control.checked ? Theme.accent : (control.down ? Theme.pressed : Theme.surfaceAlt)
        border.width: control.visualFocus ? 2 : Metrics.borderWidth
        border.color: (control.checked || control.visualFocus) ? Theme.accent : Theme.border

        Text {
            anchors.centerIn: parent
            visible: control.checkState !== Qt.Unchecked
            text: control.checkState === Qt.PartiallyChecked ? "−" : "✓"
            color: Theme.accentText
            font.pixelSize: Typography.fontCaption
        }
    }

    contentItem: Text {
        leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0
        text: control.text
        font: control.font
        color: control.enabled ? Theme.textPrimary : Theme.textMuted
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
