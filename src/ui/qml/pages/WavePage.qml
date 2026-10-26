import QtQuick
import QtQuick.Layouts

// 波形设置页骨架：波形库与编辑器在此展开。
Item {
    id: page

    objectName: "wavePage"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin
        spacing: Metrics.cardSpacing

        PageHeader {
            Layout.fillWidth: true
            title: qsTr("波形设置")
            subtitle: qsTr("波形库管理与 A/B 通道波形指派")
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Text {
                objectName: "wavePagePlaceholder"
                anchors.centerIn: parent
                text: qsTr("即将上线")
                color: Theme.textMuted
                font.pixelSize: Typography.fontSubtitle
            }
        }
    }
}
