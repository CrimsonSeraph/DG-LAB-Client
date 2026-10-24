import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Dglab

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
            title: qsTr("软件更新")

            // 标题行 + 状态徽标
            Row {
                width: parent.width
                spacing: Metrics.spacingSm

                Text {
                    text: "当前版本：v" + app.appVersion
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                }

                Item {
                    width: parent.width - 150
                    height: 1
                }

                StatusBadge {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: updateBridge.hasUpdate
                    text: "新版本 v" + updateBridge.remote_version
                    level: 1 // success 级别
                }
            }

            // 错误信息展示
            Text {
                width: parent.width
                visible: updateBridge.error_message !== ""
                text: updateBridge.error_message
                color: Theme.danger
                font.pixelSize: Typography.fontBody
                wrapMode: Text.WordWrap
            }

            // 更新日志展示
            Text {
                width: parent.width
                visible: updateBridge.hasUpdate
                text: updateBridge.release_notes
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                wrapMode: Text.WordWrap
                maximumLineCount: 4
                elide: Text.ElideRight
            }

            // 下载进度条
            ProgressBar {
                id: downloadProgress
                width: parent.width
                visible: updateBridge.downloading
                value: updateBridge.progress / 100.0
                height: 6

                background: Rectangle {
                    color: Theme.surfaceAlt
                    radius: Metrics.radiusSm
                }
                contentItem: Item {
                    Rectangle {
                        width: downloadProgress.visualPosition * parent.width
                        height: parent.height
                        color: Theme.primary
                        radius: Metrics.radiusSm
                    }
                }
            }

            // 操作按钮区域
            Row {
                width: parent.width
                spacing: Metrics.spacingMd
                layoutDirection: Qt.RightToLeft // 按钮靠右显示

                AppButton {
                    objectName: "configCheckUpdateButton"
                    text: updateBridge.checking ? "检查中..." : "检查更新"
                    enabled: !updateBridge.checking && !updateBridge.downloading
                }

                AppButton {
                    objectName: "updateDownloadButton"
                    text: "下载更新"
                    primary: true
                    visible: updateBridge.hasUpdate && !updateBridge.ready_to_install
                    enabled: !updateBridge.downloading
                }

                AppButton {
                    objectName: "updateApplyButton"
                    text: "重启并安装"
                    primary: true
                    danger: true
                    visible: updateBridge.ready_to_install
                }
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
