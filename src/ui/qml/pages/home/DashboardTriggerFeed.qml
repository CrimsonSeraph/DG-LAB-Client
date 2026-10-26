import QtQuick
import QtQuick.Layouts

// 近期触发流水卡：最近 8 条（时间 / 通道 / 模块·事件 / 强度增量），无记录时显示空态。
GlassCard {
    id: root

    objectName: "dashboardTriggerCard"
    title: qsTr("近期触发")

    readonly property int feedLimit: 8
    readonly property var entries: root.recentEntries()

    // eventBridge.triggers 按时间正序返回，倒序取最近 N 条让最新的排在最上
    function recentEntries() {
        const source = eventBridge.triggers;
        const list = [];
        const start = Math.max(0, source.length - root.feedLimit);
        for (let i = source.length - 1; i >= start; --i) {
            list.push(source[i]);
        }
        return list;
    }

    function entryTitle(item) {
        const module = (item.module === undefined || item.module === null) ? "" : String(item.module);
        const event = (item.event === undefined || item.event === null) ? "" : String(item.event);
        const option = (item.option === undefined || item.option === null) ? "" : String(item.option);
        let title = module + " · " + event;
        if (option.length > 0) {
            title += " · " + option;
        }
        return title;
    }

    Text {
        objectName: "dashboardTriggerEmpty"
        width: parent.width
        visible: root.entries.length === 0
        text: qsTr("暂无触发")
        color: Theme.textMuted
        font.pixelSize: Typography.fontBody
    }

    Column {
        width: parent.width
        spacing: Metrics.spacingSm

        Repeater {
            model: root.entries

            delegate: RowLayout {
                required property var modelData

                width: parent.width
                spacing: Metrics.spacingMd

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: modelData.time
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                    font.family: Typography.fontMonoFamily
                }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: modelData.channel
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                    font.bold: true
                }

                Text {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: root.entryTitle(modelData)
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                    elide: Text.ElideRight
                }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("Δ %1").arg(modelData.delta)
                    color: Theme.accent
                    font.pixelSize: Typography.fontBody
                    font.bold: true
                }
            }
        }
    }
}
