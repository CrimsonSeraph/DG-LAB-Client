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
    // 输出数据类型：strength 直接给强度值，percent 把强度值按数值上下限映射到 0-100，wave 让绑定波形参与叠加
    readonly property var dataTypeValues: ["strength", "percent", "wave"]
    readonly property var dataTypeModel: [qsTr("强度"), qsTr("百分比"), qsTr("波形")]

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

    function dataTypeIndex(dataType) {
        const index = root.dataTypeValues.indexOf(String(dataType));
        return index < 0 ? 0 : index;
    }

    function writeEvent(wave, frames, dataType) {
        const event = root.currentEvent;
        if (event === null || root.item === null) {
            return;
        }
        eventBridge.setEventConfig(root.item.module, event.id,
            wave === undefined ? String(event.wave) : wave,
            String(event.merge), Number(event.weight),
            frames === undefined ? Number(event.frames) : frames,
            dataType === undefined ? String(event.dataType) : dataType);
    }

    // 子选项只有数值模式可改：通道由规则决定，强度数值由事件决定
    function writeOption(option, mode) {
        const event = root.currentEvent;
        if (event === null || root.item === null) {
            return;
        }
        eventBridge.setOptionOutput(root.item.module, event.id, option.id,
            String(option.channel), String(option.operation),
            mode === undefined ? String(option.mode) : mode,
            Number(option.value), Number(option.percent), Number(option.step));
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
                            onValueModified: root.writeEvent(undefined, value * 10, undefined)
                        }
                    }
                }

                AppField {
                    width: parent.width
                    label: qsTr("输出数据类型")

                    AppSegment {
                        id: dataTypeSegment

                        width: parent.width
                        model: root.dataTypeModel
                        Binding on currentIndex {
                            value: root.currentEvent === null ? 0 : root.dataTypeIndex(root.currentEvent.dataType)
                            restoreMode: Binding.RestoreNone
                        }
                        onSelected: function (index) {
                            root.writeEvent(undefined, undefined, root.dataTypeValues[index]);
                        }
                    }
                }

                Text {
                    width: parent.width
                    text: qsTr("强度：直接可用的强度值；百分比：按该数值的上下限映射到 0-100；波形：绑定波形参与叠加。")
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                    wrapMode: Text.WordWrap
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
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 0
                                    Layout.minimumWidth: Metrics.navWidth
                                    label: qsTr("数值模式")

                                    AppSegment {
                                        id: optionModeSegment

                                        width: parent.width
                                        model: [qsTr("百分比"), qsTr("数值")]
                                        Binding on currentIndex {
                                            value: String(modelData.mode) === "percent" ? 0 : 1
                                            restoreMode: Binding.RestoreNone
                                        }
                                        onSelected: function (index) {
                                            root.writeOption(modelData, index === 0 ? "percent" : "value");
                                        }
                                    }
                                }

                                AppField {
                                    Layout.preferredWidth: Metrics.navWidth
                                    Layout.minimumWidth: Metrics.navWidth
                                    label: qsTr("强度数值")

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
