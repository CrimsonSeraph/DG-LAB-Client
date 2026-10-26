import QtQuick
import QtQuick.Layouts

// 已启用插件卡：数量 + 列表（名称 / 加载状态），点击条目进入插件中心。
// 数据取 moduleBridge.modules 中 kind 为 plugin 且 enabled 的项。
GlassCard {
    id: root

    objectName: "dashboardPluginCard"
    title: qsTr("已启用插件")

    // 插件中心的页面索引（AppBridge::Page，由页面注入）
    property int pluginCenterPage: 0

    readonly property var plugins: root.enabledPlugins()

    function enabledPlugins() {
        const source = moduleBridge.modules;
        const list = [];
        for (let i = 0; i < source.length; ++i) {
            if (String(source[i].kind) === "plugin" && source[i].enabled === true) {
                list.push(source[i]);
            }
        }
        return list;
    }

    RowLayout {
        width: parent.width
        spacing: Metrics.spacingMd

        Text {
            objectName: "dashboardPluginCount"
            text: root.plugins.length
            color: Theme.accent
            font.pixelSize: Typography.fontDisplay
            font.bold: true
        }

        Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignBottom
            text: qsTr("个插件已启用")
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
            elide: Text.ElideRight
        }
    }

    Text {
        width: parent.width
        visible: root.plugins.length === 0
        text: qsTr("暂无已启用插件，可在插件中心启用")
        color: Theme.textMuted
        font.pixelSize: Typography.fontBody
        wrapMode: Text.WordWrap
    }

    Repeater {
        model: root.plugins

        delegate: Rectangle {
            id: pluginRow

            required property var modelData

            width: parent.width
            height: pluginLayout.implicitHeight + Metrics.spacingSm * 2
            radius: Metrics.radiusSm
            color: rowHover.containsMouse ? Theme.surfaceAlt : Theme.transparentFill

            RowLayout {
                id: pluginLayout

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Metrics.spacingSm
                anchors.rightMargin: Metrics.spacingSm
                spacing: Metrics.spacingMd

                Text {
                    Layout.fillWidth: true
                    text: pluginRow.modelData.name
                    color: Theme.textPrimary
                    font.pixelSize: Typography.fontBody
                    elide: Text.ElideRight
                }

                Text {
                    text: pluginRow.modelData.stateText
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                }
            }

            MouseArea {
                id: rowHover

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onClicked: {
                    if (root.pluginCenterPage > 0) {
                        app.navigate(root.pluginCenterPage);
                    }
                }
            }
        }
    }
}
