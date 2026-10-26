pragma ComponentBehavior: Bound

import QtQuick

// 药丸式页签：选中项为强调色填充 + 深色文字，未选中为透明 + 次要文字。
// 用法：AppTabs { model: ["波形", "规则"]; currentIndex: 0 }

Item {
    id: root

    property var model: []
    property int currentIndex: 0

    readonly property int count: root.model ? root.model.length : 0

    signal tabClicked(int index)

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight
    opacity: root.enabled ? 1.0 : Metrics.disabledOpacity

    Row {
        id: row

        spacing: Metrics.spacingXs

        Repeater {
            model: root.model

            delegate: Rectangle {
                id: pill

                required property int index
                required property var modelData

                readonly property bool current: root.currentIndex === pill.index

                implicitWidth: pillLabel.implicitWidth + Metrics.spacingLg * 2
                implicitHeight: ComponentStyle.inputHeight
                radius: height / 2
                color: {
                    if (!root.enabled) {
                        return Theme.surfaceDisabled;
                    }
                    if (pill.current) {
                        return area.pressed ? Theme.pressed : (area.containsMouse ? Theme.hover : Theme.accent);
                    }
                    return (area.pressed || area.containsMouse) ? Theme.surfacePressed : Theme.transparentFill;
                }
                border.width: Metrics.borderWidth
                border.color: {
                    if (!root.enabled) {
                        return Theme.borderDisabled;
                    }
                    if (pill.current) {
                        return Theme.accent;
                    }
                    return area.containsMouse ? Theme.border : Theme.divider;
                }

                Text {
                    id: pillLabel

                    anchors.centerIn: parent
                    text: pill.modelData
                    color: {
                        if (!root.enabled) {
                            return Theme.textDisabled;
                        }
                        return pill.current ? Theme.accentText : Theme.textSecondary;
                    }
                    font.pixelSize: Typography.fontBody
                    font.bold: pill.current
                }

                MouseArea {
                    id: area

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.tabClicked(pill.index)
                }
            }
        }
    }
}
