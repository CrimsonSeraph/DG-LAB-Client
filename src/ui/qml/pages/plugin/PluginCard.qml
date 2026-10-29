import QtQuick
import QtQuick.Layouts

// 插件卡：AppCard 本体 + 状态行（状态文本 + 启停 / 规则 / 配置）。
// 点卡片本体与「配置」都进入详情；启停直接写 moduleBridge；规则入口按规则图文件是否存在切换文案，
// 点击由 C++（UiConnector）按 objectName 连接并在编辑器里直达该插件图。
Item {
    id: root

    property var item: null
    property bool selected: false
    // C++ 侧按插件标识写回：该插件已有 config/rules/plugin_<标识>.json 时按钮显示「编辑规则」
    property bool hasRuleGraph: false

    signal openDetail()
    signal toggleEnabled()

    readonly property int cardGap: Metrics.spacingSm
    readonly property bool enabledState: root.item !== null && root.item.enabled === true
    readonly property bool failedState: root.item !== null
        && (root.item.state === "failed" || (root.item.error !== undefined && root.item.error !== ""))
    readonly property string stateText: root.failedState
        ? qsTr("加载失败")
        : (root.enabledState ? qsTr("运行中") : qsTr("未启用"))
    readonly property color stateColor: root.failedState
        ? Theme.danger
        : (root.enabledState ? Theme.success : Theme.textMuted)
    readonly property string cardTitle: root.item === null ? "" : String(root.item.name)
    readonly property string cardSubtitle: {
        if (root.item === null) {
            return "";
        }
        if (root.item.description !== undefined && String(root.item.description).length > 0) {
            return String(root.item.description);
        }
        return root.item.author === undefined ? "" : String(root.item.author);
    }
    readonly property var cardTags: (root.item !== null && root.item.tags !== undefined) ? root.item.tags : []
    // 侧车 banner 只是文件名：交给 modulebanner 提供器按 <插件文件名>/<头图文件名> 解析，带路径的一律不渲染
    readonly property string bannerSource: {
        if (root.item === null || root.item.banner === undefined || root.item.fileName === undefined) {
            return "";
        }
        const file = String(root.item.fileName);
        const banner = String(root.item.banner);
        if (file.length === 0 || banner.length === 0
            || banner.indexOf("/") >= 0 || banner.indexOf(":") >= 0) {
            return "";
        }
        return "image://modulebanner/" + file + "/" + banner;
    }

    implicitWidth: Metrics.navWidth + Metrics.spacing3xl * 2
    implicitHeight: cardBody.implicitHeight + statusRow.implicitHeight + root.cardGap

    Item {
        id: cardShell

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: statusRow.top
        anchors.bottomMargin: root.cardGap

        AppCard {
            id: cardBody

            anchors.fill: parent
            title: root.cardTitle
            subtitle: root.cardSubtitle
            tags: root.cardTags
            banner: root.bannerSource
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -Metrics.borderWidth
            radius: Metrics.cardRadius + Metrics.borderWidth
            color: Theme.transparentFill
            border.width: Metrics.focusRingWidth
            border.color: Theme.accent
            visible: root.selected
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.openDetail()
        }
    }

    Item {
        id: statusRow

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        implicitHeight: actions.implicitHeight
        height: actions.implicitHeight

        Text {
            anchors.left: parent.left
            anchors.right: actions.left
            anchors.rightMargin: Metrics.spacingMd
            anchors.verticalCenter: parent.verticalCenter
            text: root.stateText
            color: root.stateColor
            font.pixelSize: Typography.fontBody
            elide: Text.ElideRight
        }

        Row {
            id: actions

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Metrics.spacingMd

            AppButton {
                text: root.enabledState ? qsTr("停用") : qsTr("启用")
                onClicked: root.toggleEnabled()
            }

            // 规则入口：只有已启用插件显示；没有规则文件时是「创建规则」，已有则「编辑规则」
            AppButton {
                id: ruleEntryButton

                objectName: "pluginRuleEntryButton"
                visible: root.enabledState

                // 下面两个属性由 C++ 侧点击时读取（sender 的属性），用于定位插件图并写日志
                property string pluginModule: root.item === null || root.item.moduleName === undefined
                    ? ""
                    : String(root.item.moduleName)
                property bool ruleGraphExists: root.hasRuleGraph

                text: root.hasRuleGraph ? qsTr("编辑规则") : qsTr("创建规则")

                // 图标 + 文案：创建用加号（f067），编辑用钢笔（f304），颜色只取令牌
                contentItem: RowLayout {
                    spacing: Metrics.spacingXs

                    AppIcon {
                        icon: ruleEntryButton.ruleGraphExists ? "f304" : "f067"
                        size: Typography.fontBody
                        color: Theme.textPrimary
                    }

                    Text {
                        text: ruleEntryButton.text
                        color: Theme.textPrimary
                        font.pixelSize: Typography.fontBody
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }
            }

            AppButton {
                text: qsTr("配置")
                primary: true
                onClicked: root.openDetail()
            }
        }
    }
}
