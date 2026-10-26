pragma ComponentBehavior: Bound

import QtQuick

// 分段控件：等宽分段，选中段为强调色填充 + 深色文字，用于「% / 数值」这类二选一。
// 用法：AppSegment { model: ["%", "数值"]; currentIndex: 0 }

Item {
    id: root

    property var model: []
    property int currentIndex: 0

    readonly property int count: root.model ? root.model.length : 0
    readonly property real segmentInset: Metrics.spacing2xs
    readonly property real segmentWidth: root.count > 0
        ? (root.width - root.segmentInset * 2 - Metrics.spacingXs * (root.count - 1)) / root.count
        : 0

    signal selected(int index)

    implicitWidth: root.count > 0 ? root.count * 64 : 120
    implicitHeight: ComponentStyle.inputHeight
    opacity: root.enabled ? 1.0 : Metrics.disabledOpacity

    Rectangle {
        anchors.fill: parent
        radius: Metrics.radiusSm
        color: Theme.surfaceAlt
        border.width: Metrics.borderWidth
        border.color: root.enabled ? Theme.border : Theme.borderDisabled
    }

    Repeater {
        model: root.count

        delegate: Rectangle {
            id: segment

            required property int index

            readonly property bool current: root.currentIndex === segment.index

            x: root.segmentInset + segment.index * (root.segmentWidth + Metrics.spacingXs)
            y: root.segmentInset
            width: root.segmentWidth
            height: Math.max(0, root.height - root.segmentInset * 2)
            radius: Metrics.radiusSm
            color: {
                if (!root.enabled) {
                    return Theme.surfaceDisabled;
                }
                if (segment.current) {
                    return area.pressed ? Theme.pressed : (area.containsMouse ? Theme.hover : Theme.accent);
                }
                return (area.pressed || area.containsMouse) ? Theme.surfacePressed : Theme.transparentFill;
            }
            border.width: segment.current ? Metrics.borderWidth : 0
            border.color: Theme.accent

            Text {
                anchors.centerIn: parent
                text: root.model[segment.index]
                color: {
                    if (!root.enabled) {
                        return Theme.textDisabled;
                    }
                    return segment.current ? Theme.accentText : Theme.textSecondary;
                }
                font.pixelSize: Typography.fontBody
                font.bold: segment.current
            }

            MouseArea {
                id: area

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.selected(segment.index)
            }
        }
    }
}
