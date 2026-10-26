import QtQuick
import QtQuick.Layouts

// 插件详情：返回入口 + 头图区 + 两个页签（模式与输出 / 触发事件）。
Item {
    id: root

    property var item: null
    property var eventsItem: null
    property int activeTab: 0

    signal back()

    readonly property string moduleName: (root.item !== null && root.item.moduleName !== undefined)
        ? String(root.item.moduleName)
        : ""
    readonly property bool enabledState: root.item !== null && root.item.enabled === true
    readonly property bool hasEventData: root.eventsItem !== null
    readonly property var channelAConfig: root.channelConfig("A")
    readonly property var channelBConfig: root.channelConfig("B")
    readonly property int limitA: root.channelAConfig === null ? 0 : Number(root.channelAConfig.limit)
    readonly property int safetyA: root.channelAConfig === null ? 0 : Number(root.channelAConfig.safety)
    readonly property int limitB: root.channelBConfig === null ? 0 : Number(root.channelBConfig.limit)
    readonly property int safetyB: root.channelBConfig === null ? 0 : Number(root.channelBConfig.safety)
    // defaultChannel 取值 A / B / AB，对应下拉的 0 / 1 / 2
    readonly property string defaultChannelValue: {
        if (root.eventsItem === null || root.eventsItem.defaultChannel === undefined) {
            return "AB";
        }
        return String(root.eventsItem.defaultChannel);
    }
    readonly property int defaultChannelIndex: root.defaultChannelValue === "A"
        ? 0
        : (root.defaultChannelValue === "B" ? 1 : 2)

    function channelConfig(channel) {
        if (root.eventsItem === null || root.eventsItem.channels === undefined) {
            return null;
        }
        const config = root.eventsItem.channels[channel];
        return config === undefined ? null : config;
    }

    function channelValueForIndex(index) {
        return index === 0 ? "A" : (index === 1 ? "B" : "AB");
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.cardSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            AppButton {
                text: qsTr("← 返回")
                onClicked: root.back()
            }

            Item {
                Layout.fillWidth: true
            }
        }

        // 头图区：banner 无解析路径，统一用 surfaceAlt 铺底
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: headerColumn.implicitHeight + Metrics.spacing3xl * 2
            radius: Metrics.cardRadius
            color: Theme.surfaceAlt
            border.width: Metrics.borderWidth
            border.color: Theme.border

            ColumnLayout {
                id: headerColumn

                anchors.left: parent.left
                anchors.right: headerActions.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Metrics.spacing2xl
                anchors.rightMargin: Metrics.spacing2xl
                spacing: Metrics.spacingMd

                Text {
                    Layout.fillWidth: true
                    text: root.item === null ? "" : String(root.item.name)
                    color: Theme.textPrimary
                    font.pixelSize: Typography.fontDisplay
                    font.bold: true
                    elide: Text.ElideRight
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingSm

                    Rectangle {
                        width: Metrics.spacingSm
                        height: Metrics.spacingSm
                        radius: width / 2
                        color: root.enabledState ? Theme.success : Theme.textMuted
                    }

                    Text {
                        text: root.enabledState ? qsTr("运行中") : qsTr("未启用")
                        color: root.enabledState ? Theme.success : Theme.textMuted
                        font.pixelSize: Typography.fontBody
                    }

                    Text {
                        visible: root.item !== null
                            && root.item.version !== undefined
                            && String(root.item.version).length > 0
                        text: visible ? qsTr("v%1").arg(String(root.item.version)) : ""
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: (root.item === null || root.item.description === undefined)
                        ? ""
                        : String(root.item.description)
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                    wrapMode: Text.WordWrap
                    visible: text.length > 0
                }
            }

            RowLayout {
                id: headerActions

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: Metrics.spacing2xl
                spacing: Metrics.spacingMd

                AppButton {
                    text: root.enabledState ? qsTr("停用插件") : qsTr("启用插件")
                    danger: root.enabledState
                    onClicked: moduleBridge.togglePlugin(String(root.item.fileName))
                }
            }
        }

        AppTabs {
            Layout.fillWidth: true
            model: [qsTr("模式与输出"), qsTr("触发事件")]
            currentIndex: root.activeTab
            onTabClicked: function (index) {
                root.activeTab = index;
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 0
            spacing: Metrics.cardSpacing

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: outputColumn.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: outputColumn

                    width: parent.width
                    spacing: Metrics.cardSpacing

                    AppField {
                        Layout.fillWidth: true
                        label: qsTr("输出通道")
                        hint: root.hasEventData
                            ? qsTr("事件的默认下发通道；子选项选「跟随默认」时按此展开，AB 会在两个通道各生效一次")
                            : qsTr("该插件未声明事件，通道配置不可用。")

                        AppComboBox {
                            id: defaultChannelCombo

                            width: parent.width
                            enabled: root.hasEventData
                            model: [qsTr("通道 A"), qsTr("通道 B"), qsTr("A + B")]
                            Binding on currentIndex {
                                value: root.defaultChannelIndex
                                restoreMode: Binding.RestoreNone
                            }
                            onActivated: function (index) {
                                eventBridge.setDefaultChannel(root.moduleName, root.channelValueForIndex(index));
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: channelColumn.implicitHeight + Metrics.cardPadding * 2
                        radius: Metrics.cardRadius
                        color: Theme.surface
                        border.width: Metrics.borderWidth
                        border.color: Theme.border
                        enabled: root.hasEventData
                        opacity: root.hasEventData ? 1.0 : Metrics.disabledOpacity

                        ColumnLayout {
                            id: channelColumn

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: Metrics.cardPadding
                            spacing: Metrics.spacingMd

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("通道上限")
                                color: Theme.textPrimary
                                font.pixelSize: Typography.fontSubtitle
                                font.bold: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Metrics.spacingXl

                                AppField {
                                    Layout.fillWidth: true
                                    label: qsTr("通道 A 上限")

                                    AppSpinBox {
                                        id: limitASpin

                                        width: parent.width
                                        from: 0
                                        to: 200
                                        Binding on value {
                                            value: root.limitA
                                            when: !limitASpin.activeFocus
                                            restoreMode: Binding.RestoreNone
                                        }
                                        onValueModified: eventBridge.setChannelConfig(root.moduleName, "A", value, root.safetyA)
                                    }
                                }

                                AppField {
                                    Layout.fillWidth: true
                                    label: qsTr("通道 A 安全上限")

                                    AppSpinBox {
                                        id: safetyASpin

                                        width: parent.width
                                        from: 0
                                        to: 200
                                        Binding on value {
                                            value: root.safetyA
                                            when: !safetyASpin.activeFocus
                                            restoreMode: Binding.RestoreNone
                                        }
                                        onValueModified: eventBridge.setChannelConfig(root.moduleName, "A", root.limitA, value)
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Metrics.spacingXl

                                AppField {
                                    Layout.fillWidth: true
                                    label: qsTr("通道 B 上限")

                                    AppSpinBox {
                                        id: limitBSpin

                                        width: parent.width
                                        from: 0
                                        to: 200
                                        Binding on value {
                                            value: root.limitB
                                            when: !limitBSpin.activeFocus
                                            restoreMode: Binding.RestoreNone
                                        }
                                        onValueModified: eventBridge.setChannelConfig(root.moduleName, "B", value, root.safetyB)
                                    }
                                }

                                AppField {
                                    Layout.fillWidth: true
                                    label: qsTr("通道 B 安全上限")

                                    AppSpinBox {
                                        id: safetyBSpin

                                        width: parent.width
                                        from: 0
                                        to: 200
                                        Binding on value {
                                            value: root.safetyB
                                            when: !safetyBSpin.activeFocus
                                            restoreMode: Binding.RestoreNone
                                        }
                                        onValueModified: eventBridge.setChannelConfig(root.moduleName, "B", root.limitB, value)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            GlassCard {
                Layout.preferredWidth: Metrics.navWidth + Metrics.spacing3xl * 2
                Layout.fillHeight: true
                title: qsTr("分发范围")

                Text {
                    width: parent.width
                    text: qsTr("输出分发到全部已连接设备，本页不提供逐设备选择。")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                    wrapMode: Text.WordWrap
                }

                Text {
                    width: parent.width
                    text: qsTr("安全上限随通道上限生效，写入后由事件运行时按新上限折算。")
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                    wrapMode: Text.WordWrap
                }
            }
        }

        EventMappingPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 1
            item: root.eventsItem
        }
    }
}
