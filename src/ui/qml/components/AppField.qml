import QtQuick

// 表单行：标签在上、控件在下，间距走 Metrics，hint 为控件下方的补充说明。
// 用法：AppField { label: "地址"; hint: "局域网 IP"; AppTextField { } }

Column {
    id: root

    property string label: ""
    property string hint: ""

    default property alias contentData: controlColumn.data

    spacing: Metrics.spacingXs
    opacity: root.enabled ? 1.0 : Metrics.disabledOpacity

    HoverHandler {
        id: hoverHandler
    }

    Text {
        id: labelText

        visible: root.label.length > 0
        text: root.label
        color: (root.enabled && hoverHandler.hovered) ? Theme.textPrimary : Theme.textSecondary
        font.pixelSize: Typography.fontBody
        elide: Text.ElideRight
    }

    Column {
        id: controlColumn

        width: root.width
        spacing: Metrics.spacingXs
    }

    Text {
        id: hintText

        visible: root.hint.length > 0
        text: root.hint
        color: Theme.textMuted
        font.pixelSize: Typography.fontBody
        elide: Text.ElideRight
    }
}
