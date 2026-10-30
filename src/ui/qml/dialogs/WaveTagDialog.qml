import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 波形标签选择器：按分类分组勾选 / 取消当前波形的标签，新建标签（自动入库），删除自定义标签。
// 分类标题只读（改名与调序在波形设置页的标签分类里做）；内置标签带锁标识且不可删除；
// 官方波形上由内置表挂载的标签（locked）显示为锁定、不可取消。删除自定义标签时若仍有波形在用，
// 由 C++ 先弹确认。交互约定：本文件不写任何信号处理器；点选、新建、删除与关闭全部由
// C++（UiConnector）按 objectName 连接。
AppDialog {
    id: dialog

    objectName: "waveTagDialog"
    title: qsTr("编辑标签 · %1").arg(waveBridge.tagTargetName)
    // 令牌值是理想尺寸，由 AppDialog 钳制到视口内，避免最小窗口下对话框被裁掉底部按钮
    preferredWidth: ComponentStyle.dialogWaveSelectWidth
    preferredHeight: ComponentStyle.dialogWaveSelectHeight

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.spacingMd

        Text {
            Layout.fillWidth: true
            text: qsTr("勾选即写入该波形；取消勾选只从波形上移除，不删除标签库里的标签。")
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
            wrapMode: Text.WordWrap
        }

        // 标签按分类分组：顺序即展示顺序，每组标题只有分类名
        Flickable {
            id: tagCategoryView

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: tagCategoryColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                width: Metrics.spacingSm
                padding: 0
            }

            Column {
                id: tagCategoryColumn

                objectName: "waveTagDialogList"
                width: tagCategoryView.width
                spacing: Metrics.spacingLg

                Repeater {
                    model: waveBridge.tagCategories

                    delegate: Column {
                        id: tagCategoryBlock

                        width: tagCategoryColumn.width
                        spacing: Metrics.spacingXs

                        RowLayout {
                            width: parent.width
                            spacing: Metrics.spacingSm

                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                color: Theme.textSecondary
                                font.pixelSize: Typography.fontBody
                                elide: Text.ElideRight
                            }

                            Text {
                                visible: modelData.tags.length === 0
                                text: qsTr("暂无标签")
                                color: Theme.textMuted
                                font.pixelSize: Typography.fontBody
                            }
                        }

                        Repeater {
                            model: modelData.tags

                            delegate: Rectangle {
                                width: tagCategoryBlock.width
                                height: Metrics.buttonHeight + Metrics.spacingMd
                                radius: Metrics.radiusSm
                                color: Theme.surfaceAlt
                                border.width: Metrics.borderWidth
                                border.color: modelData.checked ? Theme.accent : Theme.divider

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: Metrics.spacingXs
                                    anchors.leftMargin: Metrics.spacingMd
                                    spacing: Metrics.spacingMd

                                    // 勾选指示：纯展示，点选由 C++ 按 objectName 连接
                                    Rectangle {
                                        implicitWidth: ComponentStyle.checkIndicatorSize
                                        implicitHeight: ComponentStyle.checkIndicatorSize
                                        radius: Metrics.radiusXs
                                        color: modelData.checked ? Theme.accent : Theme.surface
                                        border.width: Metrics.borderWidth
                                        border.color: modelData.checked ? Theme.accent : Theme.border

                                        Text {
                                            anchors.centerIn: parent
                                            visible: modelData.checked
                                            text: "✓"
                                            color: Theme.accentText
                                            font.pixelSize: Typography.fontBody
                                        }
                                    }

                                    // 内置标签：锁标识 + 不可删除
                                    AppIcon {
                                        visible: modelData.builtin
                                        icon: "f023"
                                        size: Typography.fontBody
                                        color: Theme.textMuted
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: Theme.textPrimary
                                        font.pixelSize: Typography.fontBody
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        visible: modelData.usage > 0
                                        text: qsTr("%1 个波形在用").arg(modelData.usage)
                                        color: Theme.textMuted
                                        font.pixelSize: Typography.fontBody
                                    }

                                    // 内置表挂在官方波形上的标签：锁定，热区不可点
                                    StatusBadge {
                                        visible: modelData.locked
                                        text: qsTr("锁定")
                                        level: 2
                                    }

                                    StatusBadge {
                                        visible: modelData.builtin
                                        text: qsTr("内置")
                                        level: 0
                                    }

                                    AppButton {
                                        objectName: "waveTagDeleteClick"
                                        visible: !modelData.builtin
                                        property string tagName: modelData.name
                                        text: qsTr("删除")
                                        danger: true
                                        outline: true
                                    }
                                }

                                MouseArea {
                                    objectName: "waveTagToggleClick"
                                    anchors.fill: parent
                                    // 让行热区位于删除按钮下层；锁定的标签不接收点选
                                    z: -1
                                    enabled: !modelData.locked
                                    property string tagName: modelData.name
                                }
                            }
                        }
                    }
                }
            }
        }

        // 新建：入库并勾选到当前波形（默认落进「用户自定义」分类，大小写不同的重名沿用库里已有的写法）
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            AppTextField {
                objectName: "waveTagDialogNewField"
                Layout.fillWidth: true
                placeholderText: qsTr("新建标签名（最多 %1 个字符）").arg(waveBridge.tagMaxLength)
            }

            AppButton {
                objectName: "waveTagDialogCreateButton"
                text: qsTr("新建并勾选")
                primary: true
            }
        }
    }

    footer: Item {
        implicitHeight: tagFooter.implicitHeight + Metrics.spacingLg + Metrics.spacingMd

        RowLayout {
            id: tagFooter
            anchors.fill: parent
            anchors.topMargin: Metrics.spacingLg
            anchors.leftMargin: Metrics.spacingMd
            anchors.rightMargin: Metrics.spacingMd
            anchors.bottomMargin: Metrics.spacingMd
            spacing: Metrics.spacingMd

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "waveTagDialogCloseButton"
                text: qsTr("完成")
                primary: true
            }
        }
    }
}
