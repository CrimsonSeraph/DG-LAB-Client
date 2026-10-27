import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Dglab

// 关于页：软件介绍、许可证与依赖说明。
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
            title: qsTr("软件信息")

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
                text: qsTr("这是一款给郊狼（DG-LAB）脉冲设备用的电脑端控制软件。手机 App 扫码即可连接，也能用蓝牙直接连上郊狼设备，连上以后由电脑统一调节输出。\n\n它能做到这些：按你设定的规则自动调整强度与波形；载入、编辑并发送波形；通过插件读取游戏和外部设备的数据，让输出跟着游戏里的情况一起变化。")
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
                text: qsTr("本软件以 GPL-3.0-only 许可证发布：可以自由使用、修改和分发，修改后的版本需以同一许可证开源。")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                wrapMode: Text.WordWrap
            }

            Text {
                objectName: "aboutDependenciesText"
                width: parent.width
                text: qsTr("用到的第三方组件主要是 Qt 6（界面、网络、蓝牙与并发），各自遵循原有许可证。")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                wrapMode: Text.WordWrap
            }

            Text {
                objectName: "aboutRepositoryText"
                width: parent.width
                text: qsTr("项目地址：github.com/CrimsonSeraph/DG-LAB-Client")
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
