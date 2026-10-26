import QtQuick
import QtQuick.Layouts

// 通道状态行：通道名 + 状态徽标 + 强度/上限（只读）+ 启停按钮。
// 启停按钮沿用旧的 channel{A,B}StartButton 名，点击仍由 C++（UiConnector）接管；
// 强度只读展示，增减与波形选择已不在主页。
RowLayout {
    id: root

    property string channel: "A"
    property int strength: 0
    property int limit: 200
    property bool running: false
    // 通道输出状态（home.channelAState / channelBState）
    property string state: "idle"

    readonly property string statusText: root.state === "running" ? qsTr("运行中")
        : (root.state === "runningNoOutput" ? qsTr("无输出") : qsTr("未运行"))
    readonly property int statusLevel: root.state === "running" ? 1
        : (root.state === "runningNoOutput" ? 3 : 0)

    width: parent ? parent.width : implicitWidth
    spacing: Metrics.spacingMd

    Text {
        objectName: "dashboardChannel" + root.channel + "Name"
        Layout.alignment: Qt.AlignVCenter
        text: qsTr("通道 %1").arg(root.channel)
        color: Theme.textPrimary
        font.pixelSize: Typography.fontSubtitle
        font.bold: true
    }

    StatusBadge {
        objectName: "dashboardChannel" + root.channel + "Badge"
        Layout.alignment: Qt.AlignVCenter
        text: root.statusText
        level: root.statusLevel
    }

    Text {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        text: qsTr("强度 %1 / 上限 %2").arg(root.strength).arg(root.limit)
        color: Theme.textSecondary
        font.pixelSize: Typography.fontBody
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
    }

    AppButton {
        objectName: "channel" + root.channel + "StartButton"
        text: root.running ? qsTr("关闭通道") : qsTr("启动通道")
        // 运行态用危险色描边而不是实心填充：卡头右侧的状态徽标才是视觉重心
        primary: !root.running
        danger: root.running
        outline: root.running
    }
}
