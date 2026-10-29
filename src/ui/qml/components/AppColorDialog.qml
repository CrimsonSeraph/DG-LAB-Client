import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 自绘取色对话框：颜色预览 + HEX 输入 + 色相/饱和度/明度三条滑杆，确定后由 C++ 写回主题。
// 交互约定：本文件不写任何信号处理器；滑杆 moved()、输入框 textEdited / editingFinished 与
// 两个按钮的点击全部由 UiConnector 按 objectName 连接，HSV 与 HEX 的换算也在 C++ 侧完成。
// 对外只有两个属性：selectedColor（预览与写回用）与 hexError（格式校验提示），都由 C++ 写入。
AppDialog {
    id: root

    // 当前颜色：打开时取传入颜色，之后随滑杆拖动或 HEX 输入实时更新
    property color selectedColor: Theme.accent
    // HEX 校验提示：非空即格式非法，界面显示错误行并禁用「确定」
    property string hexError: ""

    // 宽度沿用基类默认值，高度由内容隐式高度决定
    preferredWidth: Metrics.navWidth * 2 + Metrics.spacing3xl * 2
    title: qsTr("选择颜色")

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.spacingMd

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            // 颜色预览块：只绑 selectedColor，不参与交互
            Rectangle {
                objectName: "colorDialogPreview"
                Layout.preferredWidth: ComponentStyle.swatchMinWidth
                Layout.preferredHeight: ComponentStyle.swatchHeight
                radius: Metrics.radiusSm
                color: root.selectedColor
                border.width: Metrics.borderWidth
                border.color: Theme.border
            }

            // HEX 输入：文本由 C++ 直接读写，QML 不绑 text（绑定会被用户输入打断）
            AppTextField {
                objectName: "colorDialogHexField"
                Layout.fillWidth: true
                placeholderText: qsTr("#RRGGBB")
                maximumLength: 7
            }
        }

        // 格式错误行：hexError 为空时不可见，也不占高度
        Text {
            objectName: "colorDialogHexError"
            Layout.fillWidth: true
            visible: root.hexError.length > 0
            text: root.hexError
            color: Theme.danger
            font.pixelSize: Typography.fontBody
            wrapMode: Text.WordWrap
        }

        // 色相：0-359 度，拖动后由 C++ 重算颜色
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            Text {
                text: qsTr("色相")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                Layout.preferredWidth: 48
            }

            AppSlider {
                objectName: "colorDialogHueSlider"
                Layout.fillWidth: true
                from: 0
                to: 359
                step: 1
                showValue: true
            }
        }

        // 饱和度：0-100（百分比），左侧标签宽度与色相行一致
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            Text {
                text: qsTr("饱和度")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                Layout.preferredWidth: 48
            }

            AppSlider {
                objectName: "colorDialogSaturationSlider"
                Layout.fillWidth: true
                from: 0
                to: 100
                step: 1
                showValue: true
            }
        }

        // 明度：0-100（百分比），与饱和度同为百分比口径
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            Text {
                text: qsTr("明度")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
                Layout.preferredWidth: 48
            }

            AppSlider {
                objectName: "colorDialogValueSlider"
                Layout.fillWidth: true
                from: 0
                to: 100
                step: 1
                showValue: true
            }
        }

        // 底部动作区：取消只关闭，确定在 HEX 格式非法时置灰
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "colorDialogCancelButton"
                text: qsTr("取消")
            }

            AppButton {
                objectName: "colorDialogConfirmButton"
                text: qsTr("确定")
                primary: true
                enabled: root.hexError.length === 0
            }
        }
    }
}
