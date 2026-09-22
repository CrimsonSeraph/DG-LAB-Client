pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// 主题化下拉框：内容区 / 指示器 / 弹层 / 委托全部取自 Theme 令牌。
// 交互约定：QML 不写信号处理器，选项切换由 C++（UiConnector）按 objectName 连接。
ComboBox {
    id: control

    contentItem: Text {
        leftPadding: ComponentStyle.inputPadding
        rightPadding: control.indicator ? control.indicator.width + control.spacing : 0
        text: control.displayText
        font: control.font
        color: control.enabled ? Theme.textPrimary : Theme.textMuted
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.mirrored ? control.padding : control.width - width - control.padding
        y: control.topPadding + (control.availableHeight - height) / 2
        text: "▾"
        color: control.enabled ? Theme.textSecondary : Theme.textMuted
        font.pixelSize: Typography.fontBody
        opacity: control.enabled ? 1.0 : 0.5
    }

    delegate: ItemDelegate {
        id: optionDelegate

        required property var model
        required property int index

        width: ListView.view.width
        text: control.textRole.length > 0 ? model[control.textRole] : model
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled

        background: Rectangle {
            color: (optionDelegate.highlighted || optionDelegate.hovered) ? Theme.selectionBg : Theme.surface
        }

        contentItem: Text {
            leftPadding: ComponentStyle.inputPadding
            rightPadding: ComponentStyle.inputPadding
            text: optionDelegate.text
            font: optionDelegate.font
            color: Theme.textPrimary
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    popup: Popup {
        y: control.height
        width: control.width
        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
        padding: Metrics.borderWidth

        background: Rectangle {
            radius: Metrics.radiusSm
            color: Theme.surface
            border.width: Metrics.borderWidth
            border.color: Theme.border
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0
        }
    }

    background: Rectangle {
        implicitWidth: ComponentStyle.inputMinWidth
        implicitHeight: ComponentStyle.inputHeight
        radius: Metrics.radiusSm
        color: Theme.surfaceAlt
        border.width: Metrics.borderWidth
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}
