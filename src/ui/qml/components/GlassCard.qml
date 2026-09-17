import QtQuick
import QtQuick.Layouts

// 玻璃拟态卡片容器：统一背景 / 圆角 / 描边 / 内边距，内容通过默认属性注入。
Rectangle {
    id: root

    property string title: ""
    property bool titleVisible: root.title.length > 0

    default property alias contentData: body.data

    color: Theme.surface
    radius: Metrics.cardRadius
    border.width: Metrics.borderWidth
    border.color: Theme.border
    implicitWidth: layout.implicitWidth + Metrics.cardPadding * 2
    implicitHeight: layout.implicitHeight + Metrics.cardPadding * 2

    Column {
        id: layout

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Metrics.cardPadding
        spacing: Metrics.spacingMd

        Text {
            width: parent.width
            visible: root.titleVisible
            text: root.title
            color: Theme.textPrimary
            font.pixelSize: Typography.fontSubheading
            font.bold: true
            elide: Text.ElideRight
        }

        Column {
            id: body

            width: parent.width
            spacing: Metrics.spacingMd
        }
    }
}
