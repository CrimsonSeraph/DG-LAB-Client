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

    // 空串表示跟随模块默认通道，写入时原样落盘
    function optionChannelIndex(channel) {
        const value = String(channel);
        if (value === "A") {
            return 1;
        }
        if (value === "B") {
            return 2;
        }
        return 0;
    }

    function optionChannelValue(index) {
        return index === 1 ? "A" : (index === 2 ? "B" : "");
    }

    function writeOption(option, channel, mode, value, percent) {
        const event = root.currentEvent;
        if (event === null || root.item === null) {
            return;
        }
        eventBridge.setOptionOutput(root.item.module, event.id, option.id,
            channel === undefined ? String(option.channel) : channel,
            String(option.operation),
            mode === undefined ? String(option.mode) : mode,
            value === undefined ? Number(option.value) : value,
            percent === undefined ? Number(option.percent) : percent,
            Number(option.step));
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

            ColumnLayout {
                id: formColumn

                width: parent.width
                spacing: Metrics.cardSpacing

                RowLayout {
                    Layout.fillWidth: true
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

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingXl

                    AppField {
                        Layout.fillWidth: true
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
                        Layout.fillWidth: true
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

                        Layout.fillWidth: true
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
                                    Layout.fillWidth: true
                                    label: qsTr("事件通道")
                                    hint: qsTr("跟随默认时按模块默认通道展开，AB 会在两个通道各生效一次")

                                    AppComboBox {
                                        id: optionChannelCombo

                                        width: parent.width
                                        model: [qsTr("跟随默认"), qsTr("通道 A"), qsTr("通道 B")]
                                        Binding on currentIndex {
                                            value: root.optionChannelIndex(modelData.channel)
                                            restoreMode: Binding.RestoreNone
                                        }
                                        onActivated: function (index) {
                                            root.writeOption(modelData, root.optionChannelValue(index), undefined, undefined, undefined);
                                        }
                                    }
                                }

                                AppField {
                                    Layout.fillWidth: true
                                    label: qsTr("数值模式")

                                    AppSegment {
                                        width: parent.width
                                        model: [qsTr("百分比"), qsTr("数值")]
                                        Binding on currentIndex {
                                            value: String(modelData.mode) === "percent" ? 0 : 1
                                            restoreMode: Binding.RestoreNone
                                        }
                                        onSelected: function (index) {
                                            root.writeOption(modelData, undefined, index === 0 ? "percent" : "value", undefined, undefined);
                                        }
                                    }
                                }

                                AppField {
                                    Layout.fillWidth: true
                                    label: qsTr("强度数值")
                                    hint: qsTr("百分比取 0-100，数值取绝对值")

                                    AppSpinBox {
                                        id: optionValueSpin

                                        width: parent.width
                                        from: 0
                                        to: String(modelData.mode) === "percent" ? 100 : 200
                                        Binding on value {
                                            value: String(modelData.mode) === "percent"
                                                ? Number(modelData.percent)
                                                : Number(modelData.value)
                                            when: !optionValueSpin.activeFocus
                                            restoreMode: Binding.RestoreNone
                                        }
                                        onValueModified: root.writeOption(modelData, undefined, undefined,
                                            String(modelData.mode) === "percent" ? Number(modelData.value) : value,
                                            String(modelData.mode) === "percent" ? value : Number(modelData.percent))
                                    }
                                }
                            }
                        }
                    }
                }

                AppField {
                    Layout.fillWidth: true
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
