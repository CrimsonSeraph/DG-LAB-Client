pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects

// 卡片容器：深色表面 + 细描边；头图解码完成时图片铺满卡片、标题叠在图片底部，并在图片上
// 叠加自卡片高度约 55% 起由透明过渡到 90% 深底的渐变遮罩；banner 为空或图片加载失败时
// 退回无图的居中态（与从来没有 banner 的卡片一致，不留空白头图槽）。
// 高度由使用方决定：插件中心给统一高度令牌（ComponentStyle.pluginCardHeight），
// 卡片自身只按内容给 implicitHeight，不再因有无头图给出两档高度。
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
    readonly property real scrimOpacity: 0.9

    implicitWidth: 240
    implicitHeight: content.implicitHeight + Metrics.cardPadding * 2
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

    // 头图常态即按卡片圆角裁切：MultiEffect 用一张同尺寸同圆角的实心矩形做遮罩，
    // 因此四角在无焦点时也是圆的，与聚焦态焦点环的轮廓一致（此前只有描边按圆角绘制，
    // 图片本身按矩形铺满，四角在常态下被图片盖住）。代价是头图多渲染一层图层，
    // 图片解码完成后才开启，静态图片只付一次离屏渲染成本。
    Image {
        id: bannerImage

        anchors.fill: parent
        visible: root.hasBanner
        source: root.hasBanner ? root.banner : ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        layer.enabled: root.bannerReady
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: bannerMask
        }
    }

    // 遮罩源：只取 alpha 形状（圆角矩形），本身不参与显示
    Rectangle {
        id: bannerMask

        anchors.fill: parent
        radius: root.radius
        color: Theme.surface
        visible: false
        layer.enabled: true
    }

    Rectangle {
        anchors.fill: parent
        visible: root.bannerReady
        opacity: root.scrimOpacity
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Theme.transparentFill
            }
            GradientStop {
                position: 0.55
                color: Theme.transparentFill
            }
            GradientStop {
                position: 1.0
                color: Theme.windowBg
            }
        }
    }

    Item {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: Metrics.cardPadding

        Item {
            id: innerContainer

            width: parent.width
            // 有头图时（贴底），撑满整个高度；无头图时，高度由内部内容决定（整体居中）
            height: root.bannerReady ? parent.height : implicitHeight

            anchors.verticalCenter: root.bannerReady ? undefined : parent.verticalCenter
            anchors.top: root.bannerReady ? parent.top : undefined

            // 顶部：标题（固定位置）
            Text {
                id: titleText
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                visible: root.title.length > 0
                text: root.title
                color: Theme.textPrimary
                font.pixelSize: Typography.fontSubtitle
                font.bold: true
                horizontalAlignment: root.bannerReady ? Text.AlignLeft : Text.AlignHCenter
                elide: Text.ElideRight
            }

            // 底部：标签、状态、操作（固定贴底）
            Column {
                id: bottomContainer
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: Metrics.spacingMd

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

            // 中间：描述（自适应填充，支持多行截断）
            Text {
                id: subtitleText
                anchors.top: titleText.visible ? titleText.bottom : parent.top
                anchors.bottom: bottomContainer.visible ? bottomContainer.top : parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right

                anchors.topMargin: root.title.length > 0 ? Metrics.spacingMd : 0
                anchors.bottomMargin: bottomContainer.visible ? Metrics.spacingMd : 0

                visible: root.subtitle.length > 0
                text: root.subtitle
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                horizontalAlignment: root.bannerReady ? Text.AlignLeft : Text.AlignHCenter

                // 【多行截断配置
                wrapMode: Text.WordWrap
                maximumLineCount: 3    // 最多显示 3 行
                elide: Text.ElideRight // 达到最大行数时，末尾显示 ...
                clip: true             // 裁剪掉超出范围的文本
            }
        }
    }
}
