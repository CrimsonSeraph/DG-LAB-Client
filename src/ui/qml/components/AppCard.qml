pragma ComponentBehavior: Bound

import QtQuick

// 卡片容器：深色表面 + 细描边；banner 为空时标题居中，banner 非空时图片铺满卡片、
// 标题叠在图片底部，并在图片上叠加自卡片高度约 55% 起由透明过渡到 90% 深底的渐变遮罩。
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
    readonly property int bannerMinHeight: 140
    readonly property real scrimOpacity: 0.9

    implicitWidth: 240
    implicitHeight: root.hasBanner
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
        anchors.fill: parent
        visible: root.hasBanner
        source: root.hasBanner ? root.banner : ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
    }

    Rectangle {
        anchors.fill: parent
        visible: root.hasBanner
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
            horizontalAlignment: root.hasBanner ? Text.AlignLeft : Text.AlignHCenter
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: root.subtitle.length > 0
            text: root.subtitle
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
            horizontalAlignment: root.hasBanner ? Text.AlignLeft : Text.AlignHCenter
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
