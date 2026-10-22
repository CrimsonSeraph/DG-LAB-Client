import QtQuick
import QtQuick.Controls

// 左侧导航按钮。
// 交互约定：QML 不写 onClicked，点击由 C++（UiConnector）按 objectName 连接；
// 选中态由调用方绑定 selected（不使用 checkable，避免内部 checked 破坏绑定）。
// 状态：selected / hovered / down / enabled / activeFocus，选中态为 selectionBg + selectionText（对比度由 ThemeManager 保证）。
Button {
    id: control

    property bool selected: false

    implicitHeight: Metrics.navButtonHeight
    implicitWidth: parent ? parent.width : Metrics.navWidth
    opacity: control.enabled ? 1.0 : Metrics.disabledOpacity

    background: Rectangle {
        radius: Metrics.radiusMd
        color: {
            if (!control.enabled) {
                return Theme.surfaceDisabled;
            }
            if (control.selected) {
                return Theme.selectionBg;
            }
            if (control.down) {
                return Theme.surfacePressed;
            }
            return control.hovered ? Theme.surfaceAlt : Theme.transparentFill;
        }
        border.width: Metrics.borderWidth
        border.color: control.enabled ? Theme.transparentFill : Theme.borderDisabled

        // 焦点环
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
        leftPadding: Metrics.spacingLg
        text: control.text
        color: !control.enabled ? Theme.textDisabled : (control.selected ? Theme.selectionText : Theme.textSecondary)
        font.pixelSize: Typography.fontSubtitle
        font.bold: control.selected
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
