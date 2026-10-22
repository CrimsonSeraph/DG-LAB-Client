import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 选择波形对话框：列出波形库，点击行即指派到目标通道；也可创建新波形或删除。
Dialog {
    id: dialog

    objectName: "waveSelectDialog"
    title: qsTr("选择波形")
    modal: true
    anchors.centerIn: Overlay.overlay
    // 令牌值是理想尺寸，再钳制到视口内，避免最小窗口下对话框被裁掉底部按钮
    width: Math.min(ComponentStyle.dialogWaveSelectWidth, Overlay.overlay.width - Metrics.spacingXl * 2)
    height: Math.min(ComponentStyle.dialogWaveSelectHeight, Overlay.overlay.height - Metrics.spacingXl * 2)
    padding: Metrics.spacingMd

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

                    AppButton {
                        objectName: "waveDeleteClick"
                        property string waveName: modelData.name
                        text: qsTr("删除")
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
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            AppButton {
                objectName: "waveSelectCreateButton"
                text: qsTr("创建波形")
            }

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "waveSelectCloseButton"
                text: qsTr("关闭")
            }
        }
    }
}
