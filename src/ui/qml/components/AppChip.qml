import QtQuick

// 可选中小药丸：用于事件 / 礼物类型选择，checked 态为强调色填充 + 深色文字。
// 用法：AppChip { text: "礼物"; checked: true }

Rectangle {
    id: root

    property string text: ""
    property bool checked: false

    signal clicked()

    implicitWidth: label.implicitWidth + Metrics.spacingMd * 2
    implicitHeight: ComponentStyle.statusBadgeHeight
    radius: height / 2
    opacity: root.enabled ? 1.0 : Metrics.disabledOpacity
    color: {
        if (!root.enabled) {
            return Theme.surfaceDisabled;
        }
        if (root.checked) {
            return area.pressed ? Theme.pressed : (area.containsMouse ? Theme.hover : Theme.accent);
        }
        return (area.pressed || area.containsMouse) ? Theme.surfacePressed : Theme.surface;
    }
    border.width: Metrics.borderWidth
    border.color: {
        if (!root.enabled) {
            return Theme.borderDisabled;
        }
        return root.checked ? Theme.accent : Theme.border;
    }

    Text {
        id: label

        anchors.centerIn: parent
        text: root.text
        color: {
            if (!root.enabled) {
                return Theme.textDisabled;
            }
            return root.checked ? Theme.accentText : Theme.textSecondary;
        }
        font.pixelSize: Typography.fontBody
    }

    MouseArea {
        id: area

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
