import QtQuick
import QtQuick.Layouts

// 页面标题行：页面名（fontTitle 加粗）+ 一行副标题（承载该页关键状态）+ 右侧操作区。
// 副标题由调用方绑定；默认属性槽注入右侧操作按钮（RowLayout，spacingSm）。
// 交互约定：本文件不写任何信号处理器，按钮点击由 C++（UiConnector）按 objectName 连接。
Item {
    id: header

    objectName: "pageHeader"
    property string title: ""
    property string subtitle: ""

    default property alias actions: actionRow.data

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    RowLayout {
        id: row

        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Metrics.spacingMd

        ColumnLayout {
            Layout.fillWidth: true
            // 文字列的隐含宽度不参与分配：窗口收窄时由副标题 elide 吸收，操作区不被挤出
            Layout.preferredWidth: 0
            spacing: Metrics.pageSubtitleGap

            Text {
                objectName: "pageHeaderTitle"
                Layout.fillWidth: true
                text: header.title
                color: Theme.textPrimary
                font.pixelSize: Typography.fontTitle
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                objectName: "pageHeaderSubtitle"
                Layout.fillWidth: true
                text: header.subtitle
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                elide: Text.ElideRight
            }
        }

        RowLayout {
            id: actionRow

            spacing: Metrics.spacingSm
        }
    }
}
