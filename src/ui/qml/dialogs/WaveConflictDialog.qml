import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 波形名称冲突对话框：与波形库重名的导入条目按「覆盖 / 重命名 / 跳过」统一处理。
// 单条冲突可在输入框里改写新名称；多条冲突时输入框禁用，「重命名」按「名称 (2)」序号生成。
// 交互约定：本文件不写信号处理器，三个动作按钮由 C++（UiConnector）连接。
AppDialog {
    id: dialog

    objectName: "waveConflictDialog"
    title: qsTr("波形名称冲突")
    preferredWidth: ComponentStyle.dialogWaveSelectWidth
    closePolicy: Popup.NoAutoClose

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.spacingMd

        Text {
            objectName: "waveConflictSummaryText"
            Layout.fillWidth: true
            text: qsTr("波形库中已存在以下名称：%1").arg(waveBridge.conflictNamesText)
            color: Theme.textPrimary
            font.pixelSize: Typography.fontBody
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("重命名使用的新名称")
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
        }

        AppTextField {
            objectName: "waveConflictRenameField"
            Layout.fillWidth: true
            enabled: waveBridge.conflictCount === 1
            placeholderText: qsTr("新名称")
        }

        Text {
            objectName: "waveConflictHintText"
            Layout.fillWidth: true
            text: waveBridge.conflictCount === 1
                ? qsTr("「覆盖」替换库中同名用户波形（内置官方波形不可覆盖）；「重命名」按上方名称写入；「跳过」不导入该条。")
                : qsTr("共 %1 条重名，本次选择对整批生效：「重命名」按「名称 (2)」序号依次生成。").arg(waveBridge.conflictCount)
            color: Theme.textMuted
            font.pixelSize: Typography.fontBody
            wrapMode: Text.WordWrap
        }
    }

    footer: Item {
        implicitHeight: waveConflictFooter.implicitHeight + Metrics.spacingLg + Metrics.spacingMd

        RowLayout {
            id: waveConflictFooter
            anchors.fill: parent
            anchors.topMargin: Metrics.spacingLg
            anchors.leftMargin: Metrics.spacingMd
            anchors.rightMargin: Metrics.spacingMd
            anchors.bottomMargin: Metrics.spacingMd
            spacing: Metrics.spacingMd

            AppButton {
                objectName: "waveConflictSkipButton"
                text: qsTr("跳过")
            }

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "waveConflictOverwriteButton"
                text: qsTr("覆盖")
                danger: true
                outline: true
            }

            AppButton {
                objectName: "waveConflictRenameButton"
                text: qsTr("重命名")
                primary: true
            }
        }
    }
}
