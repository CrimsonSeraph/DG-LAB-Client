pragma ComponentBehavior: Bound

import QtQuick

// 卡片容器：深色表面 + 细描边；头图解码完成时图片铺满卡片、标题叠在图片底部，并在图片上
// 叠加自卡片高度约 55% 起由透明过渡到 90% 深底的渐变遮罩；banner 为空或图片加载失败时
// 退回无图的居中态（与从来没有 banner 的卡片一致，不留空白头图槽）。
// 用法：AppCard { title: "礼物"; banner: "qrc:/img/gift.png"; status: "进行中" }

Rectangle {
    id: root

    property string title: ""
    property string subtitle: ""
    property url banner: ""
    property var tags: []
    property string status: ""

    default property alias actions: actionsRow.data

    readonly property bool hasBanner: root.banner.toString().length > 0
    // 头图只有真正解码完成才占槽：坏图停在 Error 状态，卡片与无图时同一套居中布局
    readonly property bool bannerReady: root.hasBanner && bannerImage.status === Image.Ready
    readonly property int bannerMinHeight: 140
    readonly property real scrimOpacity: 0.9

    implicitWidth: 240
    implicitHeight: root.bannerReady
        ? Math.max(root.bannerMinHeight, content.implicitHeight + Metrics.cardPadding * 2)
        : content.implicitHeight + Metrics.cardPadding * 2
    radius: Metrics.cardRadius
    color: Theme.surface
    border.width: Metrics.borderWidth
    border.color: {
        if (!root.enabled) {
            return Theme.borderDisabled;
        }
        return hoverHandler.hovered ? Theme.accent : Theme.border;
    }
    clip: true
    opacity: root.enabled ? 1.0 : Metrics.disabledOpacity

    HoverHandler {
        id: hoverHandler
    }

    Image {
        id: bannerImage

        anchors.fill: parent
        visible: root.hasBanner
        source: root.hasBanner ? root.banner : ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
    }

    Rectangle {
        anchors.fill: parent
        visible: root.bannerReady
        opacity: root.scrimOpacity
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.transparentFill }
            GradientStop { position: 0.55; color: Theme.transparentFill }
            GradientStop { position: 1.0; color: Theme.windowBg }
        }
    }

    Column {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Metrics.cardPadding
        spacing: Metrics.spacingMd

        Text {
            width: parent.width
            visible: root.title.length > 0
            text: root.title
            color: Theme.textPrimary
            font.pixelSize: Typography.fontSubtitle
            font.bold: true
            horizontalAlignment: root.bannerReady ? Text.AlignLeft : Text.AlignHCenter
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: root.subtitle.length > 0
            text: root.subtitle
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
            horizontalAlignment: root.bannerReady ? Text.AlignLeft : Text.AlignHCenter
            elide: Text.ElideRight
        }

        Flow {
            width: parent.width
            visible: root.tags && root.tags.length > 0
            spacing: Metrics.spacingXs

            Repeater {
                model: root.tags

                delegate: Rectangle {
                    id: tag

                    required property int index
                    required property var modelData

                    implicitWidth: tagLabel.implicitWidth + Metrics.spacingMd * 2
                    implicitHeight: ComponentStyle.statusBadgeHeight
                    radius: height / 2
                    color: Theme.surfaceAlt
                    border.width: Metrics.borderWidth
                    border.color: Theme.border

                    Text {
                        id: tagLabel

                        anchors.centerIn: parent
                        text: tag.modelData
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                    }
                }
            }
        }

        Item {
            width: parent.width
            height: statusText.implicitHeight
            visible: root.status.length > 0

            Rectangle {
                id: statusDot

                width: 6
                height: 6
                radius: width / 2
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.accent
            }

            Text {
                id: statusText

                anchors.left: statusDot.right
                anchors.leftMargin: Metrics.spacingSm
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: root.status
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                elide: Text.ElideRight
            }
        }

        Row {
            id: actionsRow

            width: parent.width
            spacing: Metrics.spacingMd
        }
    }
}
