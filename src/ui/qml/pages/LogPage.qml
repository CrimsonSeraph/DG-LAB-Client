import QtQuick
import QtQuick.Layouts

// 日志页骨架：日志列表与筛选由后续任务填充。
Item {
    id: page

    objectName: "logPage"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin
        spacing: Metrics.cardSpacing

        PageHeader {
            Layout.fillWidth: true
            title: qsTr("日志")
            subtitle: qsTr("运行日志查看、筛选与导出")
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Text {
                objectName: "logPagePlaceholder"
                anchors.centerIn: parent
                text: qsTr("暂无内容")
                color: Theme.textMuted
                font.pixelSize: Typography.fontSubtitle
            }
        }
    }
}
