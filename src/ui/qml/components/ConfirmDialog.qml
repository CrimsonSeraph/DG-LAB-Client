import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 通用确认弹层：标题 + 正文 + 取消/确认，供 C++ 侧 ask_confirm 复用于所有"先确认再执行"的操作。
// 交互约定：QML 不写信号处理器，两个按钮的点击由 UiConnector 按 objectName 连接。
AppDialog {
    id: dialog

    objectName: "confirmDialog"
    // 确认弹层只能由按钮或 C++ 关闭：Esc 与点击遮罩都不关闭，避免"确认意图"被误判成取消
    closePolicy: Popup.NoAutoClose
    visible: false
    title: dialog.titleText
    preferredWidth: 420

    property string titleText
    property string messageText
    property bool acceptDanger: false

    contentItem: ColumnLayout {
        spacing: Metrics.spacingLg

        Text {
            Layout.fillWidth: true
            text: dialog.messageText
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "confirmCancelButton"
                text: qsTr("取消")
            }

            AppButton {
                objectName: "confirmAcceptButton"
                text: qsTr("确认")
                primary: !dialog.acceptDanger
                danger: dialog.acceptDanger
            }
        }
    }
}
