pragma Singleton
import QtQuick

// 字号令牌：五档阶梯（正文 → 展示）+ 等宽字体族。
// 页面与组件只引用档位名，不写字号数值。
QtObject {
    readonly property int fontBody: 12
    readonly property int fontSubtitle: 14
    readonly property int fontHeading: 16
    readonly property int fontTitle: 18
    readonly property int fontDisplay: 24

    // 等宽字体族：按平台给回退链
    readonly property string fontMonoFamily: Qt.platform.os === "windows" ? "Consolas" : (Qt.platform.os === "osx" ? "Menlo" : "monospace")
}
