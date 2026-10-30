import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 迷你窗：无边框置顶小窗。内容自下而上依次为标题条、A/B 强度块（大数字、上限、占比、状态、
// 强度增减与通道启停）、透明度与大小档位、近 5 条触发流水、A/B 滚动波形。
// 交互约定：按钮与滑杆全部由 C++（UiConnector）按 objectName 连接，QML 不写信号处理器；
// 标题条拖动是登记在案的指针手势例外；波形滚动由 C++ 的 100 ms 计时器推进，QML 不开计时器。
Window {
    id: root

    objectName: "miniWindow"
    // Tool 去掉任务栏与 Alt-Tab 条目；无边框 + 置顶
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    // 圆角外的区域透出桌面：窗口本身不着色
    color: "transparent"
    // 宽度取大小档位，高度按内容 implicitHeight 计算并设上限：滑杆与列表不撑高窗口
    width: root.tierWidth
    height: Math.min(contentBody.implicitHeight + Metrics.spacingLg * 2, root.maxContentHeight)
    // 透明度只由 home 驱动，滑杆写入 home 后经绑定生效
    opacity: home.miniWindowOpacity
    visible: false
    title: qsTr("迷你窗")

    // 三档宽度（逻辑像素）：小 280 / 中 340 / 大 420
    readonly property int tierWidth: home.miniWindowSize === "small" ? 280
        : (home.miniWindowSize === "large" ? 420 : 340)
    // 内容高度上限：窗口再高就不再跟着内容长
    readonly property int maxContentHeight: 640
    readonly property int triggerLimit: 5
    readonly property int triggerRowHeight: Typography.fontBody + Metrics.spacingXs

    // eventBridge.triggers 按时间正序返回，倒序取最近 N 条让最新的排在最上（同首页触发流水卡）
    readonly property var triggerEntries: root.recentTriggers()

    // 取最近 N 条触发记录，最新的在前
    function recentTriggers() {
        const source = eventBridge.triggers;
        const list = [];
        const start = Math.max(0, source.length - root.triggerLimit);
        for (let i = source.length - 1; i >= start; --i) {
            list.push(source[i]);
        }
        return list;
    }

    // 拼触发条目标题：模块 · 事件（· 子选项）
    function triggerTitle(item) {
        const module = (item.module === undefined || item.module === null) ? "" : String(item.module);
        const event = (item.event === undefined || item.event === null) ? "" : String(item.event);
        const option = (item.option === undefined || item.option === null) ? "" : String(item.option);
        let title = module + " · " + event;
        if (option.length > 0) {
            title += " · " + option;
        }
        return title;
    }

    // 通道当前绑定的波形名，未绑定返回空串
    function waveName(channel) {
        const name = (channel === "B") ? waveBridge.currentB : waveBridge.currentA;
        return (name === undefined || name === null) ? "" : String(name);
    }

    // 通道滚动波形序列（C++ 每 100 ms 推进一次）
    function waveSeries(channel) {
        return (channel === "B") ? home.miniWindowWaveB : home.miniWindowWaveA;
    }

    // 单通道强度块：大数字 + 上限 + 占比 + 运行状态 + 强度增减 + 通道启停
    component ChannelStrengthBlock: ColumnLayout {
        id: block

        required property string channel
        required property int strength
        required property int limit
        required property bool running

        readonly property int percent: block.limit > 0
            ? Math.min(100, Math.round(block.strength * 100 / block.limit))
            : 0

        spacing: Metrics.spacingXs

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm

            StatusBadge {
                objectName: "miniWindowChannel" + block.channel + "Name"
                text: qsTr("通道 ") + block.channel
            }

            Text {
                objectName: "miniWindowChannel" + block.channel + "State"
                text: block.running ? qsTr("运行中") : qsTr("未运行")
                color: block.running ? Theme.success : Theme.textMuted
                font.pixelSize: Typography.fontBody
            }

            Item {
                Layout.fillWidth: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm

            Text {
                objectName: "miniWindowChannel" + block.channel + "Value"
                text: block.strength
                color: Theme.textPrimary
                font.pixelSize: Typography.fontDisplay
                font.bold: true
            }

            Text {
                objectName: "miniWindowChannel" + block.channel + "Limit"
                text: "/ " + block.limit
                color: Theme.textMuted
                font.pixelSize: Typography.fontSubtitle
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                objectName: "miniWindowChannel" + block.channel + "Percent"
                text: block.limit > 0 ? block.percent + "%" : "--"
                color: Theme.accent
                font.pixelSize: Typography.fontTitle
            }
        }

        // 强度增减与通道启停：objectName 里同时带 Strength<通道>{Increase,Decrease}、
        // Channel<通道>ToggleButton，既有解析逻辑据此判定通道与增减方向
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm

            AppButton {
                objectName: "miniWindowStrength" + block.channel + "Decrease"
                text: block.channel + "−"
                Layout.preferredWidth: Metrics.spacing3xl * 2
            }

            AppButton {
                objectName: "miniWindowStrength" + block.channel + "Increase"
                text: block.channel + "+"
                Layout.preferredWidth: Metrics.spacing3xl * 2
            }

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "miniWindowChannel" + block.channel + "ToggleButton"
                text: block.running ? qsTr("关闭通道") : qsTr("启动通道")
                danger: block.running
                outline: true
            }
        }
    }

    Rectangle {
        id: frame

        objectName: "miniWindowFrame"
        anchors.fill: parent
        radius: Metrics.radiusMd
        color: Theme.surface
        border.width: Metrics.borderWidth
        border.color: Theme.border

        ColumnLayout {
            id: contentBody

            anchors.fill: parent
            anchors.margins: Metrics.spacingLg
            spacing: Metrics.spacingMd

            // 标题条：标题 + 锁定 + 关闭；空白处为拖动热区
            Item {
                id: titleBar

                objectName: "miniWindowTitleBar"
                Layout.fillWidth: true
                Layout.preferredHeight: Metrics.buttonHeight

                // 指针手势例外：无边框窗口的拖动只能由指针按下与移动驱动
                MouseArea {
                    id: dragArea

                    anchors.fill: parent
                    enabled: !home.miniWindowLocked

                    // 按下时的指针屏幕坐标与窗口坐标。位移必须按屏幕坐标算：窗口内的相对坐标
                    // 会随窗口移动一起变化，拿它当基准会让窗口反过来追自己，表现为跟不上与来回抖动
                    property point pressCursor: Qt.point(0, 0)
                    property point pressWindow: Qt.point(0, 0)

                    onPressed: (mouse) => {
                        if (mouse.button !== Qt.LeftButton) {
                            return;
                        }
                        // 优先交给系统窗口管理器拖动：位移由系统计算，窗口与指针一比一跟手；
                        // 系统不支持时下面按屏幕坐标的兜底逻辑仍然生效，两条路径算的是同一个落点
                        root.startSystemMove();
                        const scene = dragArea.mapToItem(null, mouse.x, mouse.y);
                        dragArea.pressCursor = Qt.point(root.x + scene.x, root.y + scene.y);
                        dragArea.pressWindow = Qt.point(root.x, root.y);
                    }
                    onPositionChanged: (mouse) => {
                        if (!mouse.pressed) {
                            return;
                        }
                        // 指针屏幕坐标 = 窗口坐标加窗口内相对坐标，该值与窗口是否已经移动无关
                        const scene = dragArea.mapToItem(null, mouse.x, mouse.y);
                        root.x = dragArea.pressWindow.x + (root.x + scene.x) - dragArea.pressCursor.x;
                        root.y = dragArea.pressWindow.y + (root.y + scene.y) - dragArea.pressCursor.y;
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: Metrics.spacingSm

                    Text {
                        objectName: "miniWindowTitleText"
                        text: qsTr("迷你窗")
                        color: home.miniWindowLocked ? Theme.textMuted : Theme.textPrimary
                        font.pixelSize: Typography.fontSubtitle
                        font.bold: true
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    AppButton {
                        objectName: "miniWindowLockButton"
                        text: home.miniWindowLocked ? qsTr("解锁") : qsTr("锁定")
                        Layout.preferredWidth: Metrics.spacing3xl * 2
                    }

                    AppButton {
                        objectName: "miniWindowCloseButton"
                        text: "×"
                    }
                }
            }

            ChannelStrengthBlock {
                Layout.fillWidth: true
                channel: "A"
                strength: home.strengthA
                limit: home.limitA
                running: home.runningA
            }

            ChannelStrengthBlock {
                Layout.fillWidth: true
                channel: "B"
                strength: home.strengthB
                limit: home.limitB
                running: home.runningB
            }

            // 透明度与大小档位：透明度拖动中即时生效，写盘由 C++ 的 400 ms 合并计时器统一处理
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingXs

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingSm

                    Text {
                        text: qsTr("透明度")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                    }

                    AppSlider {
                        objectName: "miniWindowOpacitySlider"
                        Layout.fillWidth: true
                        from: 0.3
                        to: 1.0
                        step: 0.05
                        showValue: true
                        value: home.miniWindowOpacity
                        valueText: Math.round(home.miniWindowOpacity * 100) + "%"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingSm

                    Text {
                        text: qsTr("大小")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                    }

                    AppButton {
                        objectName: "miniWindowSizeSmallButton"
                        text: qsTr("小")
                        primary: home.miniWindowSize === "small"
                        Layout.fillWidth: true
                    }

                    AppButton {
                        objectName: "miniWindowSizeMediumButton"
                        text: qsTr("中")
                        primary: home.miniWindowSize === "medium"
                        Layout.fillWidth: true
                    }

                    AppButton {
                        objectName: "miniWindowSizeLargeButton"
                        text: qsTr("大")
                        primary: home.miniWindowSize === "large"
                        Layout.fillWidth: true
                    }
                }
            }

            // 近 N 条触发流水：列表限高可滚，条目高度之和超过上限时在列表内部滚动
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingXs

                Text {
                    text: qsTr("近期触发")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                }

                Text {
                    objectName: "miniWindowTriggerEmpty"
                    Layout.fillWidth: true
                    visible: root.triggerEntries.length === 0
                    text: qsTr("暂无触发")
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                }

                ListView {
                    id: triggerList

                    objectName: "miniWindowTriggerList"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(triggerList.contentHeight,
                        root.triggerLimit * root.triggerRowHeight)
                    visible: root.triggerEntries.length > 0
                    clip: true
                    model: root.triggerEntries
                    spacing: Metrics.spacingXs

                    delegate: RowLayout {
                        required property var modelData

                        width: triggerList.width
                        spacing: Metrics.spacingSm

                        Text {
                            text: modelData.time
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                            font.family: Typography.fontMonoFamily
                        }

                        Text {
                            text: modelData.channel
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.triggerTitle(modelData)
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }

                        Text {
                            text: qsTr("Δ %1").arg(modelData.delta)
                            color: Theme.accent
                            font.pixelSize: Typography.fontBody
                            font.bold: true
                        }
                    }
                }
            }

            // A/B 滚动波形：曲线取 waveBridge.amplitudeA/B（每帧强度字节均值 0~100）的滚动窗口
            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingMd

                Repeater {
                    model: ["A", "B"]

                    delegate: ColumnLayout {
                        id: waveColumn

                        required property string modelData

                        readonly property bool bound: root.waveName(waveColumn.modelData).length > 0

                        Layout.fillWidth: true
                        Layout.preferredWidth: 0
                        spacing: Metrics.spacingXs

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Metrics.spacingXs

                            Text {
                                text: qsTr("通道 %1").arg(waveColumn.modelData)
                                color: Theme.textSecondary
                                font.pixelSize: Typography.fontBody
                                font.bold: true
                            }

                            Text {
                                objectName: "miniWindowWaveName" + waveColumn.modelData
                                Layout.fillWidth: true
                                visible: waveColumn.bound
                                text: root.waveName(waveColumn.modelData)
                                color: Theme.textMuted
                                font.pixelSize: Typography.fontBody
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            Layout.preferredHeight: ComponentStyle.wavePreviewHeight

                            AppSparkline {
                                objectName: "miniWindowWaveSparkline" + waveColumn.modelData
                                anchors.fill: parent
                                visible: waveColumn.bound
                                points: root.waveSeries(waveColumn.modelData)
                                maxValue: 100
                            }

                            Text {
                                objectName: "miniWindowWaveEmpty" + waveColumn.modelData
                                anchors.centerIn: parent
                                visible: !waveColumn.bound
                                text: qsTr("未绑定波形")
                                color: Theme.textMuted
                                font.pixelSize: Typography.fontBody
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
