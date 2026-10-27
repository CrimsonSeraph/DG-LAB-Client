import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 通用确认弹层：标题 + 正文 + 取消/确认，供 C++ 侧 ask_confirm 复用于所有"先确认再执行"的操作。
// 交互约定：QML 不写信号处理器，两个按钮的点击与键位都由 UiConnector 接管；
// 键位语义为 Enter 确认、Esc 取消（eventFilter 在弹层可见时转入两个按钮的槽）。
AppDialog {
    id: dialog

    objectName: "confirmDialog"
    // 确认弹层只能由按钮、键位或 C++ 关闭：关闭不落在 Popup 自己手里，避免关掉弹层却留下未清空的待确认动作
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

            // 默认焦点按安全性分配：危险操作落在取消上，普通操作落在确认上（空白键只作用于该按钮）
            AppButton {
                objectName: "confirmCancelButton"
                text: qsTr("取消")
                focus: dialog.visible && dialog.acceptDanger
            }

            AppButton {
                objectName: "confirmAcceptButton"
                text: qsTr("确认")
                primary: !dialog.acceptDanger
                danger: dialog.acceptDanger
                focus: dialog.visible && !dialog.acceptDanger
            }
        }
    }
}
