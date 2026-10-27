import QtQuick
import QtQuick.Layouts

// 插件详情：返回入口 + 头图区 + 三个页签（模式与输出 / 触发事件 / 数值）。
Item {
    id: root

    objectName: "pluginDetail"

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

    // 通道上限是全局值：当前插件没有事件项时，从任一事件项取同一份全局通道配置
    readonly property var fallbackChannels: {
        const list = eventBridge.events;
        for (let i = 0; i < list.length; ++i) {
            if (list[i].channels !== undefined) {
                return list[i].channels;
            }
        }
        return null;
    }

    function channelConfig(channel) {
        const source = (root.eventsItem !== null && root.eventsItem.channels !== undefined)
            ? root.eventsItem.channels
            : root.fallbackChannels;
        if (source === null || source[channel] === undefined) {
            return null;
        }
        return source[channel];
    }

    function channelValueForIndex(index) {
        return index === 0 ? "A" : (index === 1 ? "B" : "AB");
    }

    // 选中插件已注册的数值项，由 C++ 侧 selectModule 后的刷新驱动
    readonly property var valueList: moduleBridge.selectedValues

    // 数值当前值：尚未上报时给占位符，不用 0 冒充
    function valueText(entry) {
        if (entry === null || entry.value === undefined || entry.value === null) {
            return qsTr("暂无");
        }
        return String(entry.value);
    }

    // 数值上下限：未配置的一端显示破折号，不按 0 处理
    function rangeText(entry) {
        if (entry === null) {
            return qsTr("范围未知");
        }
        const hasMin = entry.min !== undefined && entry.min !== null;
        const hasMax = entry.max !== undefined && entry.max !== null;
        if (!hasMin && !hasMax) {
            return qsTr("范围未知");
        }
        return (hasMin ? String(entry.min) : "—") + " ~ " + (hasMax ? String(entry.max) : "—");
    }

    // 数值查询周期文本，未注册周期时显示为空
    function periodText(entry) {
        if (entry === null || entry.periodText === undefined || entry.periodText === null) {
            return "";
        }
        return String(entry.periodText);
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
            model: [qsTr("模式与输出"), qsTr("触发事件"), qsTr("数值")]
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
                            : qsTr("该插件未声明事件，默认输出通道不可用。")

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

                        ColumnLayout {
                            id: channelColumn

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: Metrics.cardPadding
                            spacing: Metrics.spacingMd

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("通道上限（全局）")
                                color: Theme.textPrimary
                                font.pixelSize: Typography.fontSubtitle
                                font.bold: true
                            }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("上限与安全上限都是全局通道设置，所有插件共用，改动立即对所有插件生效。")
                                color: Theme.textMuted
                                font.pixelSize: Typography.fontBody
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("通道 A（全局）")
                                color: Theme.textSecondary
                                font.pixelSize: Typography.fontBody
                                font.bold: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Metrics.spacingXl

                                AppField {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 0
                                    label: qsTr("上限")

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
                                        onValueModified: eventBridge.setChannelConfig("A", value, root.safetyA)
                                    }
                                }

                                AppField {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 0
                                    label: qsTr("安全上限")

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
                                        onValueModified: eventBridge.setChannelConfig("A", root.limitA, value)
                                    }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("通道 B（全局）")
                                color: Theme.textSecondary
                                font.pixelSize: Typography.fontBody
                                font.bold: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Metrics.spacingXl

                                AppField {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 0
                                    label: qsTr("上限")

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
                                        onValueModified: eventBridge.setChannelConfig("B", value, root.safetyB)
                                    }
                                }

                                AppField {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 0
                                    label: qsTr("安全上限")

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
                                        onValueModified: eventBridge.setChannelConfig("B", root.limitB, value)
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
                    text: qsTr("两个上限都是全局设置：主页与状态栏显示 min(上限, 安全上限)，写入后由事件运行时按新上限折算。")
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

        // 数值页签：列出选中插件已注册的数值项与当前值，空态给明确文案
        Flickable {
            objectName: "pluginValuesPanel"

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 2
            contentWidth: width
            contentHeight: valueColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: valueColumn

                width: parent.width
                spacing: Metrics.spacingSm

                Text {
                    width: valueColumn.width
                    text: qsTr("%1 · 已注册 %2 个数值").arg(moduleBridge.selectedDisplayName).arg(root.valueList.length)
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                    elide: Text.ElideRight
                    visible: root.valueList.length > 0
                }

                Repeater {
                    model: root.valueList

                    delegate: Rectangle {
                        required property var modelData

                        width: valueColumn.width
                        implicitHeight: valueRow.implicitHeight + Metrics.spacingMd * 2
                        radius: Metrics.radiusSm
                        color: Theme.surfaceAlt
                        border.width: Metrics.borderWidth
                        border.color: Theme.divider

                        RowLayout {
                            id: valueRow

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: Metrics.spacingMd
                            spacing: Metrics.spacingMd

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 0
                                spacing: Metrics.spacingXs

                                Text {
                                    Layout.fillWidth: true
                                    text: String(modelData.name)
                                    color: Theme.textPrimary
                                    font.pixelSize: Typography.fontBody
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: String(modelData.id)
                                    color: Theme.textMuted
                                    font.pixelSize: Typography.fontBody
                                    elide: Text.ElideRight
                                }
                            }

                            Text {
                                text: root.valueText(modelData)
                                color: Theme.accent
                                font.pixelSize: Typography.fontSubtitle
                                font.bold: true
                            }

                            Text {
                                text: root.rangeText(modelData)
                                color: Theme.textMuted
                                font.pixelSize: Typography.fontBody
                            }

                            Text {
                                Layout.preferredWidth: Metrics.navWidth
                                text: root.periodText(modelData)
                                color: Theme.textSecondary
                                font.pixelSize: Typography.fontBody
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                }

                Rectangle {
                    objectName: "pluginValuesEmpty"

                    width: valueColumn.width
                    implicitHeight: valueEmptyColumn.implicitHeight + Metrics.cardPadding * 2
                    visible: root.valueList.length === 0
                    radius: Metrics.radiusSm
                    color: Theme.surfaceAlt
                    border.width: Metrics.borderWidth
                    border.color: Theme.divider

                    ColumnLayout {
                        id: valueEmptyColumn

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: Metrics.cardPadding
                        spacing: Metrics.spacingSm

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("该插件尚未注册数值")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontSubtitle
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("启用插件并由数据源上报后，这里会逐项列出数值名称、当前值与查询周期。")
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }
        }
    }
}
