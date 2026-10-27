import QtQuick

// 插件卡：AppCard 本体 + 状态行（状态文本 + 启停 / 配置）。
// 点卡片本体与「配置」都进入详情；启停直接写 moduleBridge。
// 根用普通 Item：位置器与布局的 implicit 尺寸只读，Item 的 implicit 尺寸可作为父布局的期望高度。
Item {
    id: root

    property var item: null
    property bool selected: false

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

            AppButton {
                text: qsTr("配置")
                primary: true
                onClicked: root.openDetail()
            }
        }
    }
}
