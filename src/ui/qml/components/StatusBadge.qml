import QtQuick

// 状态徽标：胶囊标签，底色统一 surfaceAlt，文字与描边按 level 取语义色。
// level：0 信息 / 1 成功 / 2 警示 / 3 危险；信息态用次级文字色 + 常规描边，不抢视觉。
// 交互约定：纯展示控件，无信号处理器；objectName 由调用方给出。
Rectangle {
    id: root

    property string text: ""
    property int level: 0

    readonly property color levelColor: {
        if (root.level === 1) {
            return Theme.success;
        }
        if (root.level === 2) {
            return Theme.warning;
        }
        if (root.level === 3) {
            return Theme.danger;
        }
        return Theme.textSecondary;
    }

    implicitWidth: label.implicitWidth + Metrics.spacingMd * 2
    implicitHeight: ComponentStyle.statusBadgeHeight
    radius: Metrics.radiusSm
    color: Theme.surfaceAlt
    border.width: Metrics.borderWidth
    border.color: root.level === 0 ? Theme.border : root.levelColor

    Text {
        id: label

        anchors.centerIn: parent
        text: root.text
        color: root.levelColor
        font.pixelSize: Typography.fontBody
    }
}
