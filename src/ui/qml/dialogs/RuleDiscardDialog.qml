import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 规则图编辑器关闭前的三选确认：保存并关闭 / 放弃改动 / 取消。
// 交互约定与 ConfirmDialog 一致：QML 不写信号处理器，三个按钮由 UiConnector 按 objectName 连接；
// 键位由 UiConnector::eventFilter 在弹层可见时接管——Esc = 取消，Enter = 保存并关闭。
AppDialog {
    id: dialog

    objectName: "ruleDiscardDialog"
    // 三选必须由按钮或键位收尾：不让 Popup 自己关掉，避免留下未决的关闭动作
    closePolicy: Popup.NoAutoClose
    visible: false
    title: qsTr("保存未保存的改动？")
    preferredWidth: 460

    /// @brief 哪些图有改动（C++ 侧在打开前写入）
    property string messageText

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

            // 默认焦点落在「取消」上：三选里唯一不改变磁盘与草稿状态的选项
            AppButton {
                objectName: "ruleDiscardCancelButton"
                text: qsTr("取消")
                focus: dialog.visible
            }

            AppButton {
                objectName: "ruleDiscardDiscardButton"
                text: qsTr("放弃改动")
                danger: true
                outline: true
            }

            AppButton {
                objectName: "ruleDiscardSaveButton"
                text: qsTr("保存并关闭")
                primary: true
            }
        }
    }
}
