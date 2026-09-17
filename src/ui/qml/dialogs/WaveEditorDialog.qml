import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 创建/编辑波形对话框：左侧段落编辑，右侧实时预览、原始 V3 帧与发送/保存。
Dialog {
    id: dialog

    objectName: "waveEditorDialog"
    title: qsTr("创建波形")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 900
    height: 640
    padding: Metrics.spacingMd

    RowLayout {
        anchors.fill: parent
        spacing: Metrics.spacingLg

        // ---------------------------------------------------------- 左：段落
        ColumnLayout {
            Layout.preferredWidth: 350
            Layout.fillHeight: true
            spacing: Metrics.spacingMd

            Text {
                text: qsTr("波形名称")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
            }

            TextField {
                id: nameField

                objectName: "waveNameField"
                Layout.fillWidth: true
                text: waveBridge.draftName
            }

            Text {
                text: qsTr("段落（点击选中后编辑参数）")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
            }

            ListView {
                id: sectionList

                objectName: "waveSectionList"
                Layout.fillWidth: true
                Layout.preferredHeight: 160
                clip: true
                model: waveBridge.draftSections
                spacing: Metrics.spacing2xs

                delegate: Rectangle {
                    width: sectionList.width
                    height: 30
                    radius: Metrics.radiusXs
                    color: modelData.selected ? Theme.selectionBg : Theme.surfaceAlt
                    border.width: Metrics.borderWidth
                    border.color: modelData.selected ? Theme.accent : Theme.divider

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: Metrics.spacingMd
                        anchors.verticalCenter: parent.verticalCenter
                        text: "#" + (modelData.index + 1) + "  " + modelData.summary
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontCaption
                    }

                    MouseArea {
                        objectName: "waveSectionClick"
                        anchors.fill: parent
                        property int sectionIndex: modelData.index
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingMd

                AppButton {
                    objectName: "waveAddSectionButton"
                    text: qsTr("添加段落")
                }

                AppButton {
                    objectName: "waveRemoveSectionButton"
                    text: qsTr("删除段落")
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            GridLayout {
                columns: 2
                columnSpacing: Metrics.spacingMd
                rowSpacing: Metrics.spacingSm

                Text {
                    text: qsTr("时长 (ms)")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontSmall
                }

                SpinBox {
                    id: durationSpin

                    objectName: "waveSectionDurationSpin"
                    from: 100
                    to: 60000
                    stepSize: 100
                    editable: true
                    Layout.fillWidth: true

                    Binding {
                        target: durationSpin
                        property: "value"
                        value: waveBridge.sectionDuration
                    }
                }

                Text {
                    text: qsTr("起始频率 (Hz)")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontSmall
                }

                SpinBox {
                    id: freqStartSpin

                    objectName: "waveSectionFreqStartSpin"
                    from: 10
                    to: 240
                    editable: true
                    Layout.fillWidth: true

                    Binding {
                        target: freqStartSpin
                        property: "value"
                        value: waveBridge.sectionFreqStart
                    }
                }

                Text {
                    text: qsTr("结束频率 (Hz)")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontSmall
                }

                SpinBox {
                    id: freqEndSpin

                    objectName: "waveSectionFreqEndSpin"
                    from: 10
                    to: 240
                    editable: true
                    Layout.fillWidth: true

                    Binding {
                        target: freqEndSpin
                        property: "value"
                        value: waveBridge.sectionFreqEnd
                    }
                }

                Text {
                    text: qsTr("起始强度")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontSmall
                }

                SpinBox {
                    id: strengthStartSpin

                    objectName: "waveSectionStrengthStartSpin"
                    from: 0
                    to: 100
                    editable: true
                    Layout.fillWidth: true

                    Binding {
                        target: strengthStartSpin
                        property: "value"
                        value: waveBridge.sectionStrengthStart
                    }
                }

                Text {
                    text: qsTr("结束强度")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontSmall
                }

                SpinBox {
                    id: strengthEndSpin

                    objectName: "waveSectionStrengthEndSpin"
                    from: 0
                    to: 100
                    editable: true
                    Layout.fillWidth: true

                    Binding {
                        target: strengthEndSpin
                        property: "value"
                        value: waveBridge.sectionStrengthEnd
                    }
                }
            }

            AppButton {
                objectName: "waveApplySectionButton"
                Layout.fillWidth: true
                text: qsTr("应用段落参数")
            }

            Item {
                Layout.fillHeight: true
            }
        }

        // ---------------------------------------------------- 右：预览与帧
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Metrics.spacingMd

            WavePreview {
                Layout.fillWidth: true
                Layout.preferredHeight: 160
                points: waveBridge.draftPoints
            }

            Text {
                text: qsTr("%1 帧 · %2ms（每帧 100ms）").arg(waveBridge.draftFrameCount).arg(waveBridge.draftDurationMs)
                color: Theme.textMuted
                font.pixelSize: Typography.fontCaption
            }

            Text {
                text: qsTr("原始 V3 帧（每行一个 8 字节 HEX，可粘贴后应用）")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextArea {
                    id: rawArea

                    objectName: "waveRawFramesArea"
                    wrapMode: TextArea.NoWrap
                    font.family: "Consolas"
                    font.pixelSize: Typography.fontSmall
                    text: waveBridge.draftFramesText

                    Binding {
                        target: rawArea
                        property: "text"
                        value: waveBridge.draftFramesText
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingMd

                AppButton {
                    objectName: "waveApplyRawButton"
                    text: qsTr("应用原始帧")
                }

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    objectName: "waveSendAButton"
                    text: qsTr("发送到 A")
                }

                AppButton {
                    objectName: "waveSendBButton"
                    text: qsTr("发送到 B")
                }

                AppButton {
                    objectName: "waveSaveButton"
                    text: qsTr("保存到波形库")
                    primary: true
                }
            }

            AppButton {
                objectName: "waveEditorCloseButton"
                Layout.fillWidth: true
                text: qsTr("关闭")
            }
        }
    }
}
