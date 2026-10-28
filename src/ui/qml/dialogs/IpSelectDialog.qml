import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 选择网卡 IP 对话框：左右两列维护黑/白名单，下方列出过滤后的候选地址，点行即选中。
// 交互约定：本文件不写任何信号处理器；点击、输入与选中全部由 C++（UiConnector）按 objectName 驱动。
AppDialog {
    id: dialog

    objectName: "ipSelectDialog"
    title: qsTr("选择网卡 IP")
    // 待写入地址字段的候选（C++ 侧在点击行时写入，确定后才落到地址与二维码）
    property string pendingIp: ""
    // 宽度取「列表对话框 + 侧栏」两枚令牌之和，容纳左右两列；高度沿用编辑器对话框令牌
    preferredWidth: ComponentStyle.dialogWaveSelectWidth + ComponentStyle.dialogSidePanelWidth
    preferredHeight: ComponentStyle.dialogWaveEditorHeight

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.spacingMd

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingLg

            // 左列：黑名单（网卡名或描述包含即忽略）
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Metrics.spacingXs

                Text {
                    text: qsTr("黑名单关键词（网卡名或描述包含则忽略）")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                }

                ListView {
                    objectName: "ipBlacklistList"
                    Layout.fillWidth: true
                    Layout.preferredHeight: ComponentStyle.inputHeight * 3
                    clip: true
                    model: device.ipSelector.blacklist
                    spacing: Metrics.spacing2xs

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: ComponentStyle.dialogListRowHeight
                        radius: Metrics.radiusSm
                        color: Theme.surfaceAlt
                        border.width: Metrics.borderWidth
                        border.color: Theme.divider

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: Metrics.spacingMd
                            anchors.right: blackRemoveButton.left
                            anchors.rightMargin: Metrics.spacingSm
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData
                            color: Theme.textPrimary
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }

                        AppButton {
                            id: blackRemoveButton

                            objectName: "ipBlacklistRemoveClick"
                            property string keyword: modelData
                            anchors.right: parent.right
                            anchors.rightMargin: Metrics.spacingXs
                            anchors.verticalCenter: parent.verticalCenter
                            height: ComponentStyle.dialogListRowHeight - Metrics.spacingXs
                            text: qsTr("删除")
                            danger: true
                            outline: true
                        }
                    }

                    Text {
                        objectName: "ipBlacklistEmpty"
                        anchors.centerIn: parent
                        visible: device.ipSelector.blacklist.length === 0
                        text: qsTr("黑名单为空，所有网卡都会进入候选")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingXs

                    AppTextField {
                        objectName: "ipBlacklistField"
                        Layout.fillWidth: true
                        placeholderText: qsTr("输入关键词")
                    }

                    AppButton {
                        objectName: "ipBlacklistAddButton"
                        text: qsTr("添加")
                    }
                }
            }

            // 右列：白名单（匹配即优先选择）
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Metrics.spacingXs

                Text {
                    text: qsTr("白名单关键词（匹配则优先选择）")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                }

                ListView {
                    objectName: "ipWhitelistList"
                    Layout.fillWidth: true
                    Layout.preferredHeight: ComponentStyle.inputHeight * 3
                    clip: true
                    model: device.ipSelector.whitelist
                    spacing: Metrics.spacing2xs

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: ComponentStyle.dialogListRowHeight
                        radius: Metrics.radiusSm
                        color: Theme.surfaceAlt
                        border.width: Metrics.borderWidth
                        border.color: Theme.divider

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: Metrics.spacingMd
                            anchors.right: whiteRemoveButton.left
                            anchors.rightMargin: Metrics.spacingSm
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData
                            color: Theme.textPrimary
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }

                        AppButton {
                            id: whiteRemoveButton

                            objectName: "ipWhitelistRemoveClick"
                            property string keyword: modelData
                            anchors.right: parent.right
                            anchors.rightMargin: Metrics.spacingXs
                            anchors.verticalCenter: parent.verticalCenter
                            height: ComponentStyle.dialogListRowHeight - Metrics.spacingXs
                            text: qsTr("删除")
                            danger: true
                            outline: true
                        }
                    }

                    Text {
                        objectName: "ipWhitelistEmpty"
                        anchors.centerIn: parent
                        visible: device.ipSelector.whitelist.length === 0
                        text: qsTr("白名单为空，按枚举顺序取第一个候选")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingXs

                    AppTextField {
                        objectName: "ipWhitelistField"
                        Layout.fillWidth: true
                        placeholderText: qsTr("输入关键词")
                    }

                    AppButton {
                        objectName: "ipWhitelistAddButton"
                        text: qsTr("添加")
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            Text {
                objectName: "ipSelectActiveText"
                text: qsTr("当前使用：%1").arg(device.ipSelector.activeIp)
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: qsTr("点击某行选中，确定后写入地址")
                color: Theme.textMuted
                font.pixelSize: Typography.fontBody
            }
        }

        // 候选地址：网卡名 / 描述 / 地址，选中行描边转 accent 并带「已选中」徽标
        ListView {
            objectName: "ipCandidateList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: device.ipSelector.candidates
            spacing: Metrics.spacingXs

            delegate: Rectangle {
                width: ListView.view.width
                height: ComponentStyle.dialogListRowHeight + Metrics.spacingSm
                radius: Metrics.radiusSm
                color: dialog.pendingIp === modelData.address ? Theme.surfaceAlt : Theme.surface
                border.width: Metrics.borderWidth
                border.color: dialog.pendingIp === modelData.address ? Theme.accent : Theme.divider

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Metrics.spacingMd
                    spacing: Metrics.spacingMd

                    Text {
                        text: modelData.name
                        color: Theme.textPrimary
                        font.pixelSize: Typography.fontBody
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.description
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }

                    StatusBadge {
                        objectName: "ipCandidateSelectedBadge"
                        visible: dialog.pendingIp === modelData.address
                        text: qsTr("已选中")
                        level: 1
                    }

                    Text {
                        text: modelData.address
                        color: Theme.accent
                        font.pixelSize: Typography.fontBody
                        font.bold: true
                    }
                }

                MouseArea {
                    objectName: "ipCandidateClick"
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    property string address: modelData.address
                }
            }

            Text {
                objectName: "ipCandidateEmpty"
                anchors.centerIn: parent
                visible: device.ipSelector.candidates.length === 0
                text: qsTr("没有可用网卡，请检查名单或在地址栏手动填写")
                color: Theme.textMuted
                font.pixelSize: Typography.fontBody
            }
        }
    }

    // 底部按钮区：恢复自动选择在左，确定 / 取消在右
    footer: Item {
        implicitHeight: ipSelectFooter.implicitHeight + Metrics.spacingLg + Metrics.spacingMd

        RowLayout {
            id: ipSelectFooter
            anchors.fill: parent
            anchors.topMargin: Metrics.spacingLg
            anchors.leftMargin: Metrics.spacingMd
            anchors.rightMargin: Metrics.spacingMd
            spacing: Metrics.spacingMd

            AppButton {
                objectName: "ipSelectAutoButton"
                text: qsTr("恢复自动选择")
            }

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "ipSelectConfirmButton"
                text: qsTr("确定")
                primary: true
                enabled: dialog.pendingIp.length > 0
            }

            AppButton {
                objectName: "ipSelectCancelButton"
                text: qsTr("取消")
            }
        }
    }
}
