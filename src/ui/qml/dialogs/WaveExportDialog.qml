import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 导出波形对话框：先选格式（JSON / .pulse）与范围（当前波形 / 全部波形），
// 确认后由 C++ 打开系统保存对话框；不可导出的组合就地给出原因并禁用确认按钮。
// 交互约定：本文件不写信号处理器，分段选择与确认按钮由 C++（UiConnector）连接。
AppDialog {
    id: dialog

    objectName: "waveExportDialog"
    title: qsTr("导出波形")
    preferredWidth: ComponentStyle.dialogWaveSelectWidth

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.spacingMd

        Text {
            Layout.fillWidth: true
            text: qsTr("格式")
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
        }

        AppSegment {
            objectName: "waveExportFormatSegment"
            Layout.fillWidth: true
            model: [qsTr("JSON"), qsTr(".pulse")]
            currentIndex: waveBridge.exportFormatIndex
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("范围")
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
        }

        AppSegment {
            objectName: "waveExportScopeSegment"
            Layout.fillWidth: true
            model: [qsTr("当前波形"), qsTr("全部波形")]
            currentIndex: waveBridge.exportScopeIndex
        }

        // 说明行同时承担「为什么不能导出」：可选时用弱化色，不可选时用警示色
        Text {
            objectName: "waveExportHintText"
            Layout.fillWidth: true
            text: waveBridge.exportHint
            color: waveBridge.exportAllowed ? Theme.textMuted : Theme.warning
            font.pixelSize: Typography.fontBody
            wrapMode: Text.WordWrap
        }
    }

    footer: Item {
        implicitHeight: waveExportFooter.implicitHeight + Metrics.spacingLg + Metrics.spacingMd

        RowLayout {
            id: waveExportFooter
            anchors.fill: parent
            anchors.topMargin: Metrics.spacingLg
            anchors.leftMargin: Metrics.spacingMd
            anchors.rightMargin: Metrics.spacingMd
            anchors.bottomMargin: Metrics.spacingMd
            spacing: Metrics.spacingMd

            AppButton {
                objectName: "waveExportCancelButton"
                text: qsTr("取消")
            }

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "waveExportConfirmButton"
                text: qsTr("导出")
                primary: true
                enabled: waveBridge.exportAllowed
            }
        }
    }
}
