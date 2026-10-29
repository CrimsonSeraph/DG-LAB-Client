import QtQuick
import "TagStripMath.js" as StripMath

// 可横向滚动的标签条：标签与波形名同一行、靠右排列；宽度不够时裁剪，滚轮或按住左右拖动查看其余部分。
// 滚动的数值计算全部在 TagStripMath.js 的纯函数里（夹取两端、滚轮步进、拖动换算），本文件只负责取数。
// 交互例外：滚轮与拖动只能由指针事件驱动，无法用 objectName + C++ 连接表达，
// 登记在 src/ui/README.md 的「信号处理器例外」表。
Item {
    id: root

    property var tags: []
    // 请求的横向偏移（0 贴着左端）；实际显示位置经 StripMath.clampOffset 夹在 [−maxOffset, 0]
    property real requestedOffset: 0

    readonly property bool hasTags: root.tags !== undefined && root.tags !== null && root.tags.length > 0
    readonly property real contentWidth: tagRow.implicitWidth
    readonly property real maxOffset: StripMath.maxOffset(root.contentWidth, root.width)
    readonly property bool scrollable: root.maxOffset > 0

    implicitWidth: root.contentWidth
    implicitHeight: ComponentStyle.statusBadgeHeight
    visible: root.hasTags
    clip: true

    Row {
        id: tagRow

        // 位置来自纯函数夹取结果：内容变宽或视口变宽时偏移自动回到合法范围
        x: StripMath.clampOffset(root.requestedOffset, root.contentWidth, root.width)
        height: root.height
        spacing: Metrics.spacingXs

        Repeater {
            model: root.tags

            delegate: StatusBadge {
                text: modelData
                level: 1
            }
        }
    }

    MouseArea {
        id: pointerArea

        // 按下时的指针起点与偏移，供拖动位移换算
        property real pressX: 0
        property real pressOffset: 0

        anchors.fill: parent
        enabled: root.scrollable
        cursorShape: root.scrollable ? Qt.OpenHandCursor : Qt.ArrowCursor

        // 滚轮：按像素增量横向移动标签；标签全部可见时不接管滚轮，页面继续滚动
        onWheel: function(wheel) {
            if (!root.scrollable) {
                return;
            }
            const delta = wheel.angleDelta.x !== 0 ? wheel.angleDelta.x : wheel.angleDelta.y;
            root.requestedOffset = StripMath.wheelOffset(tagRow.x, delta, root.contentWidth, root.width);
            wheel.accepted = true;
        }

        // 按下：记录指针起点与当前已显示的偏移
        onPressed: function(mouse) {
            pointerArea.pressX = mouse.x;
            pointerArea.pressOffset = tagRow.x;
        }

        // 拖动：按水平位移移动标签，两端由纯函数夹紧
        onPositionChanged: function(mouse) {
            if (!mouse.pressed) {
                return;
            }
            root.requestedOffset = StripMath.dragOffset(pointerArea.pressOffset, mouse.x - pointerArea.pressX, root.contentWidth, root.width);
        }
    }
}
