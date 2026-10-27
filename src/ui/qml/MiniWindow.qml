import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 迷你窗：无边框置顶小窗，本批提供 A/B 强度大数字、占比与运行状态，以及拖动 / 锁定 / 隐藏。
// 交互约定：锁定与关闭按钮由 C++（UiConnector）按 objectName 连接；标题条拖动是登记在案的指针手势例外。
// 结构预留：通道块之后按 ColumnLayout 顺序追加滑块 / 触发流水 / 波形播放（下一批）。
Window {
    id: root

    objectName: "miniWindow"
    // Tool 去掉任务栏与 Alt-Tab 条目；无边框 + 置顶
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    // 圆角外的区域透出桌面：窗口本身不着色
    color: "transparent"
    width: 340
    height: 220
    visible: false
    title: qsTr("迷你窗")

    // 单通道强度块：大数字 + 上限 + 占比 + 运行状态，全部只读绑定 home
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

                    property real press_x: 0
                    property real press_y: 0
                    property real window_x: 0
                    property real window_y: 0

                    onPressed: (mouse) => {
                        dragArea.press_x = mouse.x
                        dragArea.press_y = mouse.y
                        dragArea.window_x = root.x
                        dragArea.window_y = root.y
                    }
                    onPositionChanged: (mouse) => {
                        root.x = dragArea.window_x + (mouse.x - dragArea.press_x)
                        root.y = dragArea.window_y + (mouse.y - dragArea.press_y)
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

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
