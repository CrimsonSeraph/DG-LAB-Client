pragma Singleton
import QtQuick

// 通用设计令牌：间距 / 圆角 / 描边 / 跨组件共用的尺寸。
// 分工：颜色 -> Theme（C++ 单例）；字号 -> Typography；断点与派生 -> Responsive；
//      组件专属度量 -> ComponentStyle。页面里不要再写字面量。
QtObject {
    // ---------------------------------------------------------------- 间距刻度
    readonly property int spacing2xs: 2
    readonly property int spacingXs: 4
    readonly property int spacingSm: 6
    readonly property int spacingMd: 8
    readonly property int spacingLg: 12
    readonly property int spacingXl: 16
    readonly property int spacing2xl: 20
    readonly property int spacing3xl: 28

    // ------------------------------------------------------------ 圆角 / 描边
    readonly property int radiusXs: 3
    readonly property int radiusSm: 5
    readonly property int radiusMd: 8
    readonly property int radiusLg: 12
    readonly property int borderWidth: 1

    // ---------------------------------------------------------------- 导航栏
    readonly property int navWidth: 190
    readonly property int navButtonHeight: 42
    readonly property int navHeaderHeight: 96

    // ------------------------------------------------------------ 页面 / 卡片
    readonly property int pageMargin: 16
    readonly property int cardPadding: 14
    readonly property int cardRadius: 12
    readonly property int cardSpacing: 12
    readonly property int buttonHeight: 34
    readonly property int statusBarHeight: 28
}
