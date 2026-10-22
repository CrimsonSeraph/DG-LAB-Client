import QtQuick

// 一次性反馈浮层：右下角胶囊，由 AppBridge::showToast 驱动（3 秒后 text 清空自动隐藏）。
// 描边按 toastLevel 取语义色（0 info / 1 success / 2 warning / 3 danger）。
// bottomInset 由宿主给出：默认只留 spacingXl，主窗口再叠加状态栏高度，避免压住常驻摘要。
// 交互约定：本文件不写任何信号处理器；显示/隐藏完全由属性绑定决定。
Rectangle {
    id: toast

    objectName: "toast"
    property int bottomInset: Metrics.spacingXl

    anchors.right: parent.right
    anchors.bottom: parent.bottom
    anchors.rightMargin: Metrics.spacingXl
    anchors.bottomMargin: toast.bottomInset
    visible: app.toastText.length > 0
    width: Math.min(Metrics.toastMaxWidth, Math.max(Metrics.toastMinWidth, label.implicitWidth + Metrics.spacingLg * 2))
    height: label.implicitHeight + Metrics.spacingMd * 2
    radius: Metrics.radiusMd
    color: Theme.surface
    border.width: Metrics.borderWidth
    border.color: app.toastLevel === 1 ? Theme.success
        : app.toastLevel === 2 ? Theme.warning
        : app.toastLevel === 3 ? Theme.danger
        : Theme.border

    Text {
        id: label

        objectName: "toastText"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Metrics.spacingLg
        anchors.rightMargin: Metrics.spacingLg
        anchors.verticalCenter: parent.verticalCenter
        text: app.toastText
        color: Theme.textPrimary
        font.pixelSize: Typography.fontBody
        elide: Text.ElideRight
    }
}
