import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 软件更新对话框：当前版本、完整更新说明、下载进度与更新动作。
// 交互约定：QML 不写信号处理器，按钮点击由 UiConnector 按 objectName 连接；
// 关闭只隐藏窗口，下载状态保存在 UpdateBridge，重新打开后接着显示进度。
AppDialog {
    id: dialog

    objectName: "updateDialog"
    title: qsTr("软件更新")
    // 尺寸由令牌推导，由 AppDialog 钳制到视口内：说明再长也只滚动，不撑高对话框
    preferredWidth: Metrics.navWidth * 2 + Metrics.spacing3xl * 2
    preferredHeight: Metrics.navWidth * 3 + Metrics.spacing3xl * 2

    contentItem: ColumnLayout {
        spacing: Metrics.spacingMd

        // 版本行：当前版本吃满左侧，有更新时右侧补新版本徽标
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            Text {
                objectName: "updateDialogVersionText"
                Layout.fillWidth: true
                text: qsTr("当前版本 v%1").arg(app.appVersion)
                color: Theme.textPrimary
                font.pixelSize: Typography.fontBody
                elide: Text.ElideRight
            }

            StatusBadge {
                objectName: "updateDialogUpdateBadge"
                visible: updateBridge.hasUpdate
                text: qsTr("新版本 v%1").arg(updateBridge.remote_version)
                level: 1
            }
        }

        // 更新说明：全文放进可滚动容器，不设最大行数也不 elide
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Metrics.radiusSm
            color: Theme.surfaceAlt
            border.width: Metrics.borderWidth
            border.color: Theme.divider

            Flickable {
                id: notesView

                anchors.fill: parent
                anchors.margins: Metrics.spacingMd
                contentWidth: width
                contentHeight: notesColumn.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                // 竖向滚动条：说明过长时可滚动提示，句柄取 Theme 令牌、尺寸取 Metrics
                ScrollBar.vertical: ScrollBar {
                    id: notesScrollBar

                    objectName: "updateDialogNotesScrollBar"
                    policy: ScrollBar.AsNeeded
                    width: Metrics.spacingSm
                    padding: 0

                    contentItem: Rectangle {
                        implicitWidth: Metrics.spacingXs
                        implicitHeight: Metrics.spacingXs
                        radius: Metrics.radiusXs
                        color: notesScrollBar.pressed ? Theme.accent : Theme.textSecondary
                    }
                }

                Column {
                    id: notesColumn

                    width: notesView.width
                    spacing: Metrics.spacingXs

                    Text {
                        objectName: "updateDialogReleaseNotes"
                        width: parent.width
                        visible: updateBridge.release_notes !== ""
                        text: updateBridge.release_notes
                        textFormat: Text.PlainText
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        objectName: "updateDialogReleaseNotesEmpty"
                        width: parent.width
                        visible: updateBridge.release_notes === ""
                        text: qsTr("暂无更新说明")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        // 错误行：只在有错误时占位，避免空行吃掉内容区高度
        Text {
            objectName: "updateDialogErrorText"
            Layout.fillWidth: true
            visible: updateBridge.error_message !== ""
            text: updateBridge.error_message
            color: Theme.danger
            font.pixelSize: Typography.fontBody
            wrapMode: Text.WordWrap
        }

        // 下载进度：进度值来自 UpdateBridge，关闭对话框不中断下载
        ProgressBar {
            id: downloadProgress

            objectName: "updateDialogProgress"
            Layout.fillWidth: true
            visible: updateBridge.downloading
            value: updateBridge.progress / 100.0
            implicitHeight: Metrics.spacingSm

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
    }

    // 底部按钮区：上行检查更新与更新说明链接，下行更新动作与关闭
    footer: Item {
        implicitHeight: footerColumn.implicitHeight + Metrics.spacingLg + Metrics.spacingMd

        ColumnLayout {
            id: footerColumn

            anchors.fill: parent
            anchors.topMargin: Metrics.spacingLg
            anchors.leftMargin: Metrics.spacingMd
            anchors.rightMargin: Metrics.spacingMd
            anchors.bottomMargin: Metrics.spacingMd
            spacing: Metrics.spacingMd

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingMd

                AppButton {
                    objectName: "updateDialogCheckButton"
                    text: updateBridge.checking ? qsTr("检查中…") : qsTr("检查更新")
                    enabled: !updateBridge.checking && !updateBridge.downloading
                }

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    objectName: "updateDialogReleaseUrlButton"
                    text: qsTr("查看完整更新说明")
                    visible: updateBridge.release_url !== ""
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingMd

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    objectName: "updateDialogDownloadButton"
                    text: updateBridge.downloading ? qsTr("下载中…") : qsTr("下载更新")
                    primary: true
                    visible: updateBridge.hasUpdate && !updateBridge.ready_to_install
                    enabled: !updateBridge.downloading
                }

                AppButton {
                    objectName: "updateDialogApplyButton"
                    text: qsTr("重启并安装")
                    danger: true
                    outline: true
                    visible: updateBridge.ready_to_install
                }

                AppButton {
                    objectName: "updateDialogCloseButton"
                    text: qsTr("关闭")
                }
            }
        }
    }
}
