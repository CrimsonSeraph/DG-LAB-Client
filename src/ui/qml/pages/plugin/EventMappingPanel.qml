pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 触发事件：事件芯片 + 选中事件的映射表单（事件级 + 子选项级）。
// 所有写入都走 eventBridge，回显一律读桥接刷新后的数据。
Item {
    id: root

    property var item: null
    property string selectedEventId: ""

    readonly property var eventList: (root.item !== null && root.item.events !== undefined) ? root.item.events : []
    readonly property var currentEvent: root.findEvent(root.selectedEventId)
    readonly property var optionList: (root.currentEvent !== null && root.currentEvent.options !== undefined)
        ? root.currentEvent.options
        : []
    // 触发数值的输出上下限与运行期当前值，全部来自桥接的只读字段；
    // 数值未注册或未配置上下限时不给 0，直接显示范围未知
    readonly property string outputRangeText: {
        const event = root.currentEvent;
        if (event === null) {
            return "";
        }
        const valueKnown = event.valueKnown === true;
        const minKnown = valueKnown && event.valueMinKnown === true;
        const maxKnown = valueKnown && event.valueMaxKnown === true;
        if (!minKnown && !maxKnown) {
            return qsTr("范围未知");
        }
        const current = event.valueCurrentKnown === true ? String(Number(event.valueCurrent)) : qsTr("暂无");
        if (minKnown && maxKnown) {
            return qsTr("输出范围 %1 ~ %2，当前 %3")
                .arg(Number(event.valueMin))
                .arg(Number(event.valueMax))
                .arg(current);
        }
        if (maxKnown) {
            return qsTr("输出范围 ≤ %1，当前 %2").arg(Number(event.valueMax)).arg(current);
        }
        return qsTr("输出范围 ≥ %1，当前 %2").arg(Number(event.valueMin)).arg(current);
    }

    readonly property var waveNames: {
        const names = [];
        const source = waveBridge.waves;
        for (let i = 0; i < source.length; ++i) {
            names.push(String(source[i].name));
        }
        return names;
    }
    readonly property var waveModel: [qsTr("默认（按当前设备）")].concat(root.waveNames)
    // 波形帧预览暂无接口，先给固定示例点
    readonly property var previewPoints: [8, 22, 35, 28, 46, 38, 58, 44, 62, 51, 70, 60, 74, 66, 55, 48, 36, 30, 22, 14]

    function findEvent(eventId) {
        for (let i = 0; i < root.eventList.length; ++i) {
            if (String(root.eventList[i].id) === String(eventId)) {
                return root.eventList[i];
            }
        }
        return root.eventList.length > 0 ? root.eventList[0] : null;
    }

    function waveIndex(waveName) {
        for (let i = 0; i < root.waveNames.length; ++i) {
            if (root.waveNames[i] === String(waveName)) {
                return i + 1;
            }
        }
        return 0;
    }

    function writeEvent(enabled, wave, frames) {
        const event = root.currentEvent;
        if (event === null || root.item === null) {
            return;
        }
        eventBridge.setEventConfig(root.item.module, event.id,
            enabled === undefined ? event.enabled === true : enabled,
            wave === undefined ? String(event.wave) : wave,
            String(event.merge), Number(event.weight),
            frames === undefined ? Number(event.frames) : frames);
    }

    // 子选项映射只读展示：通道与强度都由事件和规则决定，界面不提供写入入口
    function optionChannelText(channel) {
        const value = String(channel);
        if (value === "A") {
            return qsTr("通道 A");
        }
        if (value === "B") {
            return qsTr("通道 B");
        }
        return qsTr("跟随默认通道");
    }

    function optionModeText(mode) {
        return String(mode) === "percent"
            ? qsTr("百分比（按 0-100 映射实际值）")
            : qsTr("数值（直接使用实际值）");
    }

    function optionValueText(option) {
        return String(option.mode) === "percent"
            ? qsTr("%1 / 100").arg(Number(option.percent))
            : String(Number(option.value));
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.cardSpacing

        Text {
            Layout.fillWidth: true
            visible: root.eventList.length === 0
            text: qsTr("该插件未声明可配置的事件")
            color: Theme.textMuted
            font.pixelSize: Typography.fontBody
        }

        Flow {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm

            Repeater {
                model: root.eventList

                delegate: AppChip {
                    required property var modelData

                    text: String(modelData.label)
                    checked: root.currentEvent !== null && String(root.currentEvent.id) === String(modelData.id)
                    onClicked: root.selectedEventId = modelData.id
                }
            }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.currentEvent !== null
            contentWidth: width
            contentHeight: formColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: formColumn

                width: parent.width
                spacing: Metrics.cardSpacing

                RowLayout {
                    width: parent.width
                    spacing: Metrics.spacingMd

                    Text {
                        Layout.fillWidth: true
                        text: root.currentEvent === null ? "" : String(root.currentEvent.label)
                        color: Theme.textPrimary
                        font.pixelSize: Typography.fontSubtitle
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        text: root.currentEvent === null ? "" : qsTr("触发：%1 · 阈值 %2")
                            .arg(String(root.currentEvent.trigger.value))
                            .arg(Number(root.currentEvent.trigger.amount))
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        elide: Text.ElideRight
                    }
                }

                Text {
                    width: parent.width
                    text: root.outputRangeText
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    width: parent.width
                    spacing: Metrics.spacingXl

                    AppField {
                        Layout.preferredWidth: Metrics.navWidth
                        Layout.minimumWidth: Metrics.navWidth
                        label: qsTr("启用事件")

                        AppSwitch {
                            id: eventEnabledSwitch

                            Binding on checked {
                                value: root.currentEvent !== null && root.currentEvent.enabled === true
                                restoreMode: Binding.RestoreNone
                            }
                            onToggled: root.writeEvent(checked, undefined, undefined)
                        }
                    }

                    AppField {
                        Layout.fillWidth: true
                        Layout.minimumWidth: Metrics.navWidth * 2
                        label: qsTr("波形")
                        hint: qsTr("默认即不附加波形")

                        AppComboBox {
                            id: waveCombo

                            width: parent.width
                            model: root.waveModel
                            Binding on currentIndex {
                                value: root.currentEvent === null ? 0 : root.waveIndex(root.currentEvent.wave)
                                restoreMode: Binding.RestoreNone
                            }
                            onActivated: function (index) {
                                root.writeEvent(undefined, index === 0 ? "" : root.waveNames[index - 1], undefined);
                            }
                        }
                    }

                    AppField {
                        Layout.preferredWidth: Metrics.navWidth
                        Layout.minimumWidth: Metrics.navWidth
                        label: qsTr("激活窗口（秒）")
                        hint: qsTr("1 秒 = 10 帧")

                        AppSpinBox {
                            id: framesSpin

                            width: parent.width
                            from: 1
                            to: 60
                            Binding on value {
                                value: root.currentEvent === null
                                    ? 1
                                    : Math.max(1, Math.round(Number(root.currentEvent.frames) / 10))
                                when: !framesSpin.activeFocus
                                restoreMode: Binding.RestoreNone
                            }
                            onValueModified: root.writeEvent(undefined, undefined, value * 10)
                        }
                    }
                }

                Repeater {
                    model: root.optionList

                    delegate: Rectangle {
                        required property var modelData

                        width: parent.width
                        implicitHeight: optionColumn.implicitHeight + Metrics.cardPadding * 2
                        radius: Metrics.cardRadius
                        color: Theme.surface
                        border.width: Metrics.borderWidth
                        border.color: Theme.border

                        ColumnLayout {
                            id: optionColumn

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: Metrics.cardPadding
                            spacing: Metrics.spacingMd

                            Text {
                                Layout.fillWidth: true
                                text: String(modelData.label) + qsTr(" · %1").arg(String(modelData.operation))
                                color: Theme.textPrimary
                                font.pixelSize: Typography.fontBody
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Metrics.spacingXl

                                AppField {
                                    Layout.preferredWidth: Metrics.navWidth
                                    Layout.minimumWidth: Metrics.navWidth
                                    label: qsTr("事件通道")

                                    Text {
                                        width: parent.width
                                        text: root.optionChannelText(modelData.channel)
                                        color: Theme.textPrimary
                                        font.pixelSize: Typography.fontBody
                                        elide: Text.ElideRight
                                    }
                                }

                                AppField {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 0
                                    Layout.minimumWidth: Metrics.navWidth
                                    label: qsTr("数值模式")

                                    Text {
                                        width: parent.width
                                        text: root.optionModeText(modelData.mode)
                                        color: Theme.textPrimary
                                        font.pixelSize: Typography.fontBody
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                AppField {
                                    Layout.preferredWidth: Metrics.navWidth
                                    Layout.minimumWidth: Metrics.navWidth
                                    label: qsTr("强度数值")
                                    hint: qsTr("由事件决定，不可手动调整")

                                    Text {
                                        width: parent.width
                                        text: root.optionValueText(modelData)
                                        color: Theme.textPrimary
                                        font.pixelSize: Typography.fontBody
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }

                AppField {
                    width: parent.width
                    label: qsTr("强度预览")
                    hint: qsTr("示例曲线")

                    AppSparkline {
                        width: parent.width
                        height: ComponentStyle.wavePreviewHeight
                        points: root.previewPoints
                        maxValue: 100
                    }
                }
            }
        }
    }
}
