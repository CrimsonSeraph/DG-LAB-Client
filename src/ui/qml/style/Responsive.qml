pragma Singleton
import QtQuick

// 断点与派生判断：断点值只在这里定义，页面通过 isXxx() 判断，避免各页各写魔法数字。
QtObject {
    readonly property int minWindowWidth: 900
    readonly property int minWindowHeight: 600
    // 宽度不足时导航栏折叠为窄条
    readonly property int compactNavWidth: 1180
    // 宽度不足时隐藏状态栏右侧信息
    readonly property int narrowWidth: 1000

    function isCompactNav(windowWidth) {
        return windowWidth < compactNavWidth;
    }

    function isNarrow(windowWidth) {
        return windowWidth < narrowWidth;
    }
}
