import QtQuick
import QtQuick.Controls

// 左侧导航按钮。
// 交互约定：QML 不写 onClicked，点击由 C++（UiConnector）按 objectName 连接；
// 选中态由调用方绑定 selected（不使用 checkable，避免内部 checked 破坏绑定）。
Button {
    id: control

    property bool selected: false

    implicitHeight: Metrics.navButtonHeight
    implicitWidth: parent ? parent.width : Metrics.navWidth

    background: Rectangle {
        color: control.selected ? Theme.selectionBg : (control.hovered ? Theme.surfaceAlt : "transparent")
        radius: Metrics.radiusMd
    }

    contentItem: Text {
        leftPadding: Metrics.spacingLg
        text: control.text
        color: control.selected ? Theme.accent : Theme.textSecondary
        font.pixelSize: Typography.fontBodyLarge
        font.bold: control.selected
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
