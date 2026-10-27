import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 选择波形对话框：列出波形库，点击行即指派到目标通道；也可创建新波形或删除。
AppDialog {
    id: dialog

    objectName: "waveSelectDialog"
    title: qsTr("选择波形")
    // 令牌值是理想尺寸，由 AppDialog 钳制到视口内，避免最小窗口下对话框被裁掉底部按钮
    preferredWidth: ComponentStyle.dialogWaveSelectWidth
    preferredHeight: ComponentStyle.dialogWaveSelectHeight

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.spacingMd

        Text {
            text: qsTr("目标通道：%1").arg(waveBridge.targetChannel)
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
        }

        ListView {
            id: waveList

            objectName: "waveLibraryList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: waveBridge.waves
            spacing: Metrics.spacingXs

            delegate: Rectangle {
                width: waveList.width
                height: 52
                radius: Metrics.radiusSm
                color: Theme.surfaceAlt
                border.width: Metrics.borderWidth
                border.color: Theme.divider

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Metrics.spacingMd
                    spacing: Metrics.spacingMd

                    Text {
                        Layout.fillWidth: true
                        text: modelData.name
                        color: Theme.textPrimary
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }

                    Text {
                        text: qsTr("%1 帧 · %2ms").arg(modelData.frameCount).arg(modelData.durationMs)
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                    }

                    // 已指派到通道的行就地标出，省得用户回首页确认
                    StatusBadge {
                        objectName: "waveCurrentABadge"
                        visible: waveBridge.currentA === modelData.name
                        text: qsTr("当前 A")
                        level: 0
                    }

                    StatusBadge {
                        objectName: "waveCurrentBBadge"
                        visible: waveBridge.currentB === modelData.name
                        text: qsTr("当前 B")
                        level: 0
                    }

                    AppButton {
                        objectName: "waveDeleteClick"
                        property string waveName: modelData.name
                        text: qsTr("删除")
                        danger: true
                    }
                }

                MouseArea {
                    objectName: "waveSelectClick"
                    anchors.fill: parent
                    // 让行热区位于删除按钮下层
                    z: -1
                    property string waveName: modelData.name
                }
            }

            // 空态：库为空时给出下一步指引
            Text {
                objectName: "waveLibraryEmptyText"
                anchors.centerIn: parent
                width: parent.width - Metrics.spacingXl * 2
                visible: waveBridge.waves.length === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("波形库为空，点击下方「创建波形」新建一个")
                color: Theme.textMuted
                font.pixelSize: Typography.fontBody
            }
        }
    }

    // 底部按钮区：左次要（关闭）、右主操作（创建波形）；
    // 间距与上边距由令牌给定，不再贴着内容区下沿
    footer: Item {
        // 上边距 = spacingLg（与内容区留白），左右与下边距 = spacingMd（与内容区内缩对齐）
        implicitHeight: waveSelectFooter.implicitHeight + Metrics.spacingLg + Metrics.spacingMd

        RowLayout {
            id: waveSelectFooter
            anchors.fill: parent
            anchors.topMargin: Metrics.spacingLg
            anchors.leftMargin: Metrics.spacingMd
            anchors.rightMargin: Metrics.spacingMd
            anchors.bottomMargin: Metrics.spacingMd
            spacing: Metrics.spacingMd

            AppButton {
                objectName: "waveSelectCloseButton"
                text: qsTr("关闭")
            }

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "waveSelectCreateButton"
                text: qsTr("创建波形")
                primary: true
            }
        }
    }
}
