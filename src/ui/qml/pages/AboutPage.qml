import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 关于页：应用信息、许可证与依赖说明。
Item {
    id: page

    objectName: "aboutPage"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin
        spacing: Metrics.cardSpacing

        PageHeader {
            Layout.fillWidth: true
            title: qsTr("关于")
            subtitle: "v" + app.appVersion
        }

        GlassCard {
            Layout.fillWidth: true
            title: app.appName

            Text {
                objectName: "aboutVersionText"
                width: parent.width
                text: qsTr("版本 %1").arg(app.appVersion)
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
            }

            // 正文与版本号同色阶：描述性文字不应比它更弱
            Text {
                objectName: "aboutDescriptionText"
                width: parent.width
                text: qsTr("一个基于 Qt 的 DG-Lab 桌面客户端：从外部数据源获取数值，经规则计算后控制设备输出。")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                wrapMode: Text.WordWrap
            }
        }

        GlassCard {
            Layout.fillWidth: true
            title: qsTr("许可证与依赖")

            Text {
                objectName: "aboutLicenseText"
                width: parent.width
                text: qsTr("许可证 GPL-3.0-only")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                wrapMode: Text.WordWrap
            }

            Text {
                objectName: "aboutDependenciesText"
                width: parent.width
                text: qsTr("依赖 Qt 6（Quick / QuickControls2 / Network / Bluetooth）")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                wrapMode: Text.WordWrap
            }

            Text {
                objectName: "aboutRepositoryText"
                width: parent.width
                text: qsTr("仓库 github.com/CrimsonSeraph/DG-LAB-Client")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                wrapMode: Text.WordWrap
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
