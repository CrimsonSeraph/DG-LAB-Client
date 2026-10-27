import QtQuick

// 图标组件：用 Font Awesome 字体渲染单个字符码点。
// 字体文件在 assets/font/，由 DGLABClient.qrc 的 /font 前缀打进 Qt 资源，
// 这里用 FontLoader 注册到字体库；solid（实心）与 regular（空心）各一个加载器。
// 用法：AppIcon { icon: "f015"; size: Typography.fontHeading }
// 交互约定：纯展示控件，无信号处理器；颜色一律取 Theme 令牌。
Item {
    id: root

    /// 字符码点：十六进制字符串（如 "f015"）或字符本身（如 "\uf015"）
    property string icon: ""
    /// 字号：同时决定图标的隐式尺寸
    property int size: Typography.fontSubtitle
    property color color: Theme.textSecondary
    /// 字体字重：solid（默认）/ regular
    property string variant: "solid"

    readonly property bool useRegular: root.variant === "regular"
    // 两个字重共用同一排版族名（FA 7 的 name ID16 均为 "Font Awesome 7 Free"），
    // 实心/常规靠字重区分：900 = Solid，400 = Regular。
    // 字体未加载完成时 name 为空，先按已知族名回退，避免短暂显示缺字方框。
    readonly property string fontFamily: solidLoader.name !== ""
        ? solidLoader.name
        : (regularLoader.name !== "" ? regularLoader.name : "Font Awesome 7 Free")

    implicitWidth: glyph.implicitWidth
    implicitHeight: glyph.implicitHeight

    /// 把十六进制码点或字符解析成可直接交给 Text 的字符串
    function glyph_text(raw) {
        if (!raw) {
            return "";
        }
        if (raw.codePointAt(0) > 0xff) {
            return raw;
        }
        const code = Number.parseInt(raw, 16);
        return Number.isNaN(code) ? raw : String.fromCodePoint(code);
    }

    FontLoader {
        id: solidLoader

        source: "qrc:/font/assets/font/fa-solid-900.otf"
    }

    FontLoader {
        id: regularLoader

        source: "qrc:/font/assets/font/fa-regular-400.otf"
    }

    Text {
        id: glyph

        anchors.centerIn: parent
        text: root.glyph_text(root.icon)
        color: root.color
        font.family: root.fontFamily
        font.weight: root.useRegular ? Font.Normal : Font.Black
        font.pixelSize: root.size
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
