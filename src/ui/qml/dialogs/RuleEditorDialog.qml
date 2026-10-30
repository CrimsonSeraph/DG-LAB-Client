import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// 规则可视化编辑器（ComfyUI / UE 蓝图风格）。
//
// 信号处理器例外：本编辑器的画布与节点手势（拖动、滚轮缩放、右键菜单、快捷键）
// 属于 src/ui/README.md 中登记的"必须使用手势的视图"，允许写处理器；处理器只改画布自身的
// 视图属性（平移、缩放、拖动增量）或调用 ruleGraph（C++）的方法，图的增删改/复制粘贴/写回
// 全部在 C++ 完成。
Window {
    id: dialog

    objectName: "ruleEditorDialog"
    title: qsTr("规则编辑器")
    // 独立窗口：原生标题栏提供拖四边缩放、最大化、最小化与关闭；
    // 尺寸下限取令牌，默认尺寸取令牌，用户拉伸后的尺寸由窗口自己保存
    width: ComponentStyle.dialogRuleEditorWidth
    height: ComponentStyle.dialogRuleEditorHeight
    minimumWidth: ComponentStyle.dialogRuleEditorMinWidth
    minimumHeight: ComponentStyle.dialogRuleEditorMinHeight
    color: Theme.surface
    visible: false

    // 当前图标题：无插件 = 总规则图，否则取插件显示名
    function currentGraphTitle() {
        var plugin = ruleGraph.currentGraphPlugin;
        if (plugin === "") {
            return qsTr("总规则图");
        }
        var list = ruleGraph.pluginGraphs;
        for (var i = 0; i < list.length; i++) {
            if (list[i].plugin === plugin) {
                return qsTr("%1 规则图").arg(list[i].label);
            }
        }
        return qsTr("%1 规则图").arg(plugin);
    }

    // 新插件输出节点按已有数量纵向错开，避免落点重叠
    function pluginOutputRow() {
        var list = ruleGraph.nodes;
        var count = 0;
        for (var i = 0; i < list.length; i++) {
            if (list[i].type === "pluginOutput") {
                ++count;
            }
        }
        return count;
    }

    // 标题行：当前图标题 + 返回总图
    Item {
        id: ruleHeaderRow

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Metrics.spacingMd
        height: ruleHeader.implicitHeight

        RowLayout {
            id: ruleHeader
            anchors.fill: parent
            spacing: Metrics.spacingMd

            Text {
                objectName: "ruleGraphTitle"
                text: dialog.currentGraphTitle()
                color: Theme.textPrimary
                font.pixelSize: Typography.fontSubtitle
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "ruleBackToTopButton"
                text: qsTr("返回总图")
                visible: ruleGraph.currentGraphPlugin !== ""
                onClicked: ruleGraph.loadPluginGraph("")
            }
        }
    }

    // 主体三栏：左栏（模块 / 添加节点）、画布、检查器
    SplitView {
        id: rootRow

        anchors.top: ruleHeaderRow.bottom
        anchors.bottom: ruleFooterRow.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Metrics.spacingMd
        orientation: Qt.Horizontal

        // 模块 / 节点面板
        ColumnLayout {
            id: leftCol
            SplitView.preferredWidth: ComponentStyle.paletteWidth
            SplitView.minimumWidth: 180
            Layout.fillHeight: true
            spacing: Metrics.spacingMd

            // 插件选择器：插件输出节点只属于总规则图，插件图里换成数值源列表
            ColumnLayout {
                id: pluginSection

                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: ruleGraph.currentGraphPlugin === ""
                spacing: Metrics.spacingMd

                Text {
                    text: qsTr("插件（点击添加输出节点）")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                }

                ListView {
                    id: pluginList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: ruleGraph.pluginGraphs
                    spacing: Metrics.spacing2xs

                    delegate: Rectangle {
                        width: pluginList.width
                        height: ComponentStyle.dialogListRowHeight
                        radius: Metrics.radiusXs
                        color: Theme.surfaceAlt
                        border.width: Metrics.borderWidth
                        border.color: Theme.divider

                        Text {
                            id: pluginBadge

                            anchors.right: parent.right
                            anchors.rightMargin: Metrics.spacingSm
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.hasGraph ? qsTr("已有规则图") : qsTr("无规则图")
                            color: modelData.hasGraph ? Theme.accent : Theme.textMuted
                            font.pixelSize: Typography.fontBody
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: Metrics.spacingSm
                            anchors.right: pluginBadge.left
                            anchors.rightMargin: Metrics.spacingSm
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label
                            color: Theme.textPrimary
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: ruleGraph.addPluginOutputNode(modelData.plugin, 100, 80 + dialog.pluginOutputRow() * 140)
                        }
                    }
                }
            }

            // 数值源列表：插件规则图的输入取自插件数值
            ColumnLayout {
                id: moduleSection

                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: ruleGraph.currentGraphPlugin !== ""
                spacing: Metrics.spacingMd

                Text {
                    text: qsTr("模块数值（点击添加源节点）")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                }

                ListView {
                    id: moduleList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: ruleGraph.moduleValues
                    spacing: Metrics.spacing2xs

                    delegate: Rectangle {
                        width: moduleList.width
                        height: ComponentStyle.dialogListRowHeight
                        radius: Metrics.radiusXs
                        color: Theme.surfaceAlt
                        border.width: Metrics.borderWidth
                        border.color: Theme.divider

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: Metrics.spacingSm
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label + "  (" + modelData.valueId + ")"
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                            width: parent.width - Metrics.spacingLg
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: ruleGraph.addModuleNode(modelData.valueId, 60, 80 + moduleList.contentY / 2)
                        }
                    }
                }
            }

            Text {
                text: qsTr("添加节点")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Metrics.spacingSm
                rowSpacing: Metrics.spacingSm

                AppButton {
                    objectName: "ruleAddAdd"
                    text: "+"
                    onClicked: ruleGraph.addOperatorNode("+", 300, 120)
                }
                AppButton {
                    objectName: "ruleAddSub"
                    text: "−"
                    onClicked: ruleGraph.addOperatorNode("-", 300, 120)
                }
                AppButton {
                    objectName: "ruleAddMul"
                    text: "×"
                    onClicked: ruleGraph.addOperatorNode("*", 300, 120)
                }
                AppButton {
                    objectName: "ruleAddDiv"
                    text: "÷"
                    onClicked: ruleGraph.addOperatorNode("/", 300, 120)
                }
                AppButton {
                    objectName: "ruleAddAbs"
                    text: qsTr("绝对值")
                    onClicked: ruleGraph.addAdvancedNode("abs", 300, 260)
                }
                AppButton {
                    objectName: "ruleAddSquare"
                    text: qsTr("平方")
                    onClicked: ruleGraph.addAdvancedNode("square", 300, 260)
                }
                AppButton {
                    objectName: "ruleAddSqrt"
                    text: qsTr("开根号")
                    onClicked: ruleGraph.addAdvancedNode("sqrt", 300, 260)
                }
                AppButton {
                    objectName: "ruleAddExpr"
                    text: qsTr("表达式")
                    onClicked: ruleGraph.addAdvancedNode("expression", 300, 260)
                }
                AppButton {
                    objectName: "ruleAddRule"
                    text: qsTr("规则节点")
                    onClicked: ruleGraph.addRuleNode(520, 120)
                }
                AppButton {
                    objectName: "ruleAddChannelA"
                    text: qsTr("通道 A")
                    onClicked: ruleGraph.addChannelNode("A", 820, 80)
                }
                AppButton {
                    objectName: "ruleAddChannelB"
                    text: qsTr("通道 B")
                    onClicked: ruleGraph.addChannelNode("B", 820, 260)
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }

        // 画布
        Rectangle {
            id: canvasRect
            SplitView.fillWidth: true
            SplitView.minimumWidth: 300
            color: Theme.surface
            radius: Metrics.radiusMd
            border.width: Metrics.borderWidth
            border.color: Theme.border
            clip: true

            Item {
                id: canvas

                // C++（UiConnector）按此名访问画布做视图适配
                objectName: "ruleCanvas"
                anchors.fill: parent

                property real panX: 40
                property real panY: 20
                property real zoom: 1.0
                property int dragNodeId: -1
                // 拖动增量（世界单位）：拖动期间节点与连线代理按它平移，模型坐标保持按下时的值
                property real dragDx: 0
                property real dragDy: 0
                // 按下时的指针世界坐标与节点世界坐标：增量的基准与松手时的落点
                property real dragOriginX: 0
                property real dragOriginY: 0
                property real dragNodeX: 0
                property real dragNodeY: 0
                property int connectFrom: -1
                property real tempX: 0
                property real tempY: 0
                property bool panning: false
                property real lastMouseX: 0
                property real lastMouseY: 0
                // 最近一次右键命中的连线 id（0 = 未命中）
                property int contextEdgeId: 0
                // 点击 / 拖动判定：按下后位移不足 clickMoveThreshold 视为点击
                readonly property real clickMoveThreshold: 4
                // 默认输出节点的引用行高度（世界单位），代理与命中测试共用
                readonly property real referenceRowHeight: Typography.fontBody + Metrics.spacingXs
                property int pressNodeId: -1
                property real pressX: 0
                property real pressY: 0
                property bool dragMoved: false

                function toWorldX(x) {
                    return (x - panX) / zoom;
                }

                function toWorldY(y) {
                    return (y - panY) / zoom;
                }

                // 节点高度：行数由 C++ 给出（输入端口 + 通道节点的目标设备行 + 默认输出节点的引用行），
                // 代理、命中测试与 C++ 的适配视图共用同一个 rowCount
                function nodeHeight(n) {
                    return n.headerHeight + n.portInset + n.rowCount * n.portSpacing + Metrics.spacingLg;
                }

                function nodeAt(wx, wy) {
                    var list = ruleGraph.nodes;
                    for (var i = list.length - 1; i >= 0; i--) {
                        var n = list[i];
                        var height = nodeHeight(n);
                        if (wx >= n.x && wx <= n.x + n.width && wy >= n.y && wy <= n.y + height) {
                            return n;
                        }
                    }
                    return null;
                }

                // 引用行在节点内的纵向起点：与节点代理的 Column 布局一致
                function referenceRowTop(n, index) {
                    return n.y + n.headerHeight + Metrics.spacingSm + (Math.max(1, n.inputCount) + index) * n.portSpacing;
                }

                // 命中的引用行下标（-1 = 未命中）
                function referenceIndexAt(n, wx, wy) {
                    if (wx < n.x + Metrics.spacingSm || wx > n.x + n.width - Metrics.spacingSm) {
                        return -1;
                    }
                    for (var i = 0; i < n.references.length; i++) {
                        var top = referenceRowTop(n, i);
                        if (wy >= top && wy <= top + referenceRowHeight) {
                            return i;
                        }
                    }
                    return -1;
                }

                // 图标识 -> 图：top 是总规则图，plugin:<插件标识> 是插件规则图
                function openGraphReference(reference) {
                    if (reference === "top") {
                        ruleGraph.loadPluginGraph("");
                    } else if (reference.indexOf("plugin:") === 0) {
                        ruleGraph.loadPluginGraph(reference.substring(7));
                    }
                }

                // 单击（未拖动）节点：插件输出节点进入插件图，默认输出节点按引用行跳转
                function handleNodeClick(nodeId, wx, wy) {
                    var list = ruleGraph.nodes;
                    for (var i = 0; i < list.length; i++) {
                        var n = list[i];
                        if (n.id !== nodeId) {
                            continue;
                        }
                        if (n.type === "pluginOutput" && n.pluginId !== "") {
                            ruleGraph.loadPluginGraph(n.pluginId);
                        } else if (n.type === "defaultOutput") {
                            var index = referenceIndexAt(n, wx, wy);
                            if (index >= 0) {
                                openGraphReference(n.references[index]);
                            }
                        }
                        return;
                    }
                }

                // 端口命中容差是屏幕像素，换算成世界单位要除以 zoom，
                // 否则 0.4 倍下命中区缩水、2 倍下过大
                function portHitTolerance() {
                    return ComponentStyle.graphPortHitTolerance / zoom;
                }

                function inputPortAt(wx, wy) {
                    var list = ruleGraph.nodes;
                    var tolerance = portHitTolerance();
                    for (var i = 0; i < list.length; i++) {
                        var n = list[i];
                        for (var p = 0; p < n.inputCount; p++) {
                            var px = n.x;
                            var py = n.y + n.headerHeight + n.portInset + p * n.portSpacing;
                            if (Math.abs(wx - px) < tolerance && Math.abs(wy - py) < tolerance) {
                                return {
                                    "node": n.id,
                                    "port": p
                                };
                            }
                        }
                    }
                    return null;
                }

                // 点到线段的最短距离（世界单位）
                function distanceToSegment(px, py, ax, ay, bx, by) {
                    var dx = bx - ax;
                    var dy = by - ay;
                    var lengthSquared = dx * dx + dy * dy;
                    if (lengthSquared <= 0) {
                        return Math.sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay));
                    }
                    var t = Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / lengthSquared));
                    var cx = ax + t * dx;
                    var cy = ay + t * dy;
                    return Math.sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
                }

                // 连线命中：把连线曲线按等参折线采样，取容差内距离最近的一条，返回连线 id（0 = 未命中）。
                // 参数是画布内（屏幕）坐标；曲线控制点与绘制使用同一组偏移，容差同样按屏幕恒定。
                function edgeAt(sx, sy) {
                    var wx = toWorldX(sx);
                    var wy = toWorldY(sy);
                    var limit = (ComponentStyle.edgeWidth + Metrics.spacingSm) / zoom;
                    var list = ruleGraph.edges;
                    var hit = 0;
                    var best = limit;
                    for (var i = 0; i < list.length; i++) {
                        var e = list[i];
                        var c1x = e.fromX + ComponentStyle.graphEdgeCurveOffset;
                        var c2x = e.toX - ComponentStyle.graphEdgeCurveOffset;
                        var px = e.fromX;
                        var py = e.fromY;
                        var segments = 24;
                        for (var s = 1; s <= segments; s++) {
                            var t = s / segments;
                            var mt = 1 - t;
                            var qx = mt * mt * mt * e.fromX + 3 * mt * mt * t * c1x + 3 * mt * t * t * c2x + t * t * t * e.toX;
                            var qy = mt * mt * mt * e.fromY + 3 * mt * mt * t * e.fromY + 3 * mt * t * t * e.toY + t * t * t * e.toY;
                            var distance = distanceToSegment(wx, wy, px, py, qx, qy);
                            if (distance <= best) {
                                best = distance;
                                hit = e.id;
                            }
                            px = qx;
                            py = qy;
                        }
                    }
                    return hit;
                }

                // 无限网格背景：网格线铺满可视区域，起点由画布平移量对步长取模得到，
                // 因此任意平移距离下网格都不会跑出视口；间距与缩放同步，缩放过小时整体隐藏
                Item {
                    id: infiniteGrid

                    anchors.fill: parent
                    z: -1

                    readonly property real gridStep: ComponentStyle.graphGridSize * canvas.zoom
                    // 取模偏移落回 (-步长, 0]，保证第一根线画在视口外一格之内，平移任意距离都不会留白
                    readonly property real offsetX: {
                        if (infiniteGrid.gridStep <= 0) {
                            return 0;
                        }
                        const remainder = canvas.panX % infiniteGrid.gridStep;
                        return remainder > 0 ? remainder - infiniteGrid.gridStep : remainder;
                    }
                    readonly property real offsetY: {
                        if (infiniteGrid.gridStep <= 0) {
                            return 0;
                        }
                        const remainder = canvas.panY % infiniteGrid.gridStep;
                        return remainder > 0 ? remainder - infiniteGrid.gridStep : remainder;
                    }
                    // 竖线与横线的条数：铺满当前视口，多留一条盖住右边缘与下边缘
                    readonly property int columnCount: infiniteGrid.gridStep <= 0
                        ? 0
                        : Math.ceil(infiniteGrid.width / infiniteGrid.gridStep) + 1
                    readonly property int rowCount: infiniteGrid.gridStep <= 0
                        ? 0
                        : Math.ceil(infiniteGrid.height / infiniteGrid.gridStep) + 1

                    visible: gridStep >= 6

                    Repeater {
                        model: infiniteGrid.columnCount

                        delegate: Rectangle {
                            x: Math.round(infiniteGrid.offsetX + index * infiniteGrid.gridStep)
                            width: Metrics.borderWidth
                            height: infiniteGrid.height
                            color: Theme.divider
                            opacity: ComponentStyle.graphGridOpacity
                        }
                    }

                    Repeater {
                        model: infiniteGrid.rowCount

                        delegate: Rectangle {
                            y: Math.round(infiniteGrid.offsetY + index * infiniteGrid.gridStep)
                            width: infiniteGrid.width
                            height: Metrics.borderWidth
                            color: Theme.divider
                            opacity: ComponentStyle.graphGridOpacity
                        }
                    }
                }

                Item {
                    id: world

                    x: canvas.panX
                    y: canvas.panY
                    width: ComponentStyle.graphWorldWidth
                    height: ComponentStyle.graphWorldHeight
                    transform: Scale {
                        origin.x: 0
                        origin.y: 0
                        xScale: canvas.zoom
                        yScale: canvas.zoom
                    }

                    // 连线
                    Repeater {
                        model: ruleGraph.edges

                        delegate: Shape {
                            id: edgeShape

                            // 端点按被拖节点跟随增量平移，控制点与端点用同一组偏移
                            readonly property real fromDx: canvas.dragNodeId === modelData.from ? canvas.dragDx : 0
                            readonly property real fromDy: canvas.dragNodeId === modelData.from ? canvas.dragDy : 0
                            readonly property real toDx: canvas.dragNodeId === modelData.to ? canvas.dragDx : 0
                            readonly property real toDy: canvas.dragNodeId === modelData.to ? canvas.dragDy : 0

                            anchors.fill: parent
                            antialiasing: true

                            ShapePath {
                                strokeColor: Theme.accent
                                strokeWidth: ComponentStyle.edgeWidth
                                fillColor: Theme.transparentFill
                                startX: modelData.fromX + edgeShape.fromDx
                                startY: modelData.fromY + edgeShape.fromDy
                                PathCubic {
                                    control1X: modelData.fromX + ComponentStyle.graphEdgeCurveOffset + edgeShape.fromDx
                                    control1Y: modelData.fromY + edgeShape.fromDy
                                    control2X: modelData.toX - ComponentStyle.graphEdgeCurveOffset + edgeShape.toDx
                                    control2Y: modelData.toY + edgeShape.toDy
                                    x: modelData.toX + edgeShape.toDx
                                    y: modelData.toY + edgeShape.toDy
                                }
                            }
                        }
                    }

                    // 临时连线
                    Shape {
                        anchors.fill: parent
                        visible: canvas.connectFrom > 0
                        antialiasing: true

                        ShapePath {
                            strokeColor: Theme.warning
                            strokeWidth: ComponentStyle.edgeWidth
                            strokeStyle: ShapePath.DashLine
                            fillColor: Theme.transparentFill
                            startX: {
                                var list = ruleGraph.nodes;
                                for (var i = 0; i < list.length; i++) {
                                    if (list[i].id === canvas.connectFrom) {
                                        return list[i].x + list[i].width;
                                    }
                                }
                                return 0;
                            }
                            startY: {
                                var list = ruleGraph.nodes;
                                for (var i = 0; i < list.length; i++) {
                                    if (list[i].id === canvas.connectFrom) {
                                        return list[i].y + list[i].headerHeight / 2;
                                    }
                                }
                                return 0;
                            }
                            PathLine {
                                x: canvas.tempX
                                y: canvas.tempY
                            }
                        }
                    }

                    // 节点
                    Repeater {
                        model: ruleGraph.nodes

                        delegate: Rectangle {
                            property var nodeData: modelData

                            x: nodeData.x + (canvas.dragNodeId === nodeData.id ? canvas.dragDx : 0)
                            y: nodeData.y + (canvas.dragNodeId === nodeData.id ? canvas.dragDy : 0)
                            width: modelData.width
                            height: canvas.nodeHeight(modelData)
                            radius: Metrics.radiusSm
                            color: Theme.surfaceAlt
                            border.width: ruleGraph.selectedNodeId === modelData.id ? 2 : Metrics.borderWidth
                            border.color: ruleGraph.selectedNodeId === modelData.id ? Theme.accent : Theme.border

                            Rectangle {
                                width: parent.width
                                height: modelData.headerHeight
                                radius: Metrics.radiusSm
                                color: ruleGraph.selectedNodeId === modelData.id ? Theme.selectionBg : Theme.surface

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: Metrics.spacingSm
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - Metrics.spacingLg
                                    text: modelData.name
                                    color: Theme.textPrimary
                                    font.pixelSize: Typography.fontBody
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                            }

                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.topMargin: modelData.headerHeight + Metrics.spacingSm
                                spacing: modelData.portSpacing - Typography.fontBody - Metrics.spacingXs

                                Repeater {
                                    model: modelData.inputCount

                                    delegate: Row {
                                        height: Typography.fontBody + Metrics.spacingXs

                                        Rectangle {
                                            width: 10
                                            height: 10
                                            radius: 5
                                            color: Theme.accent
                                            anchors.verticalCenter: parent.verticalCenter
                                            x: -5
                                        }

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            x: 10
                                            text: nodeData.inputLabels[index]
                                            color: Theme.textMuted
                                            font.pixelSize: Typography.fontBody
                                        }
                                    }
                                }

                                // 通道节点：目标设备一行（跟随默认设备 / 指定 MAC）
                                Text {
                                    visible: nodeData.type === "channel"
                                    height: Typography.fontBody + Metrics.spacingXs
                                    text: nodeData.deviceLabel
                                    color: Theme.textMuted
                                    font.pixelSize: Typography.fontBody
                                    width: nodeData.width - Metrics.spacingSm
                                    elide: Text.ElideRight
                                }

                                // 默认输出节点的引用文本（可能多条）：单击跳到对应图
                                Repeater {
                                    model: nodeData.type === "defaultOutput" ? nodeData.references : []

                                    delegate: Rectangle {
                                        x: Metrics.spacingSm
                                        width: nodeData.width - Metrics.spacingSm * 2
                                        height: canvas.referenceRowHeight
                                        radius: Metrics.radiusXs
                                        color: Theme.selectionBg
                                        border.width: Metrics.borderWidth
                                        border.color: Theme.accent

                                        Text {
                                            anchors.centerIn: parent
                                            text: nodeData.referenceLabels[index]
                                            color: Theme.accent
                                            font.pixelSize: Typography.fontBody
                                        }
                                    }
                                }
                            }

                            // 插件输出节点：单击进入该插件的规则图
                            Text {
                                visible: modelData.type === "pluginOutput"
                                anchors.left: parent.left
                                anchors.leftMargin: Metrics.spacingSm
                                anchors.top: parent.top
                                anchors.topMargin: modelData.headerHeight + Metrics.spacingSm
                                text: qsTr("单击进入插件规则图")
                                color: Theme.textMuted
                                font.pixelSize: Typography.fontBody
                            }

                            // 输出端口
                            Rectangle {
                                width: ComponentStyle.nodePortSize
                                height: ComponentStyle.nodePortSize
                                radius: ComponentStyle.nodePortSize / 2
                                color: Theme.accent
                                x: parent.width - ComponentStyle.nodePortSize / 2
                                y: modelData.headerHeight / 2 - ComponentStyle.nodePortSize / 2
                            }
                        }
                    }
                }

                // 缩放百分比：画布角落的只读指示，纯绑定 canvas.zoom
                Text {
                    objectName: "ruleZoomLabel"
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Metrics.spacingMd
                    text: Math.round(canvas.zoom * 100) + "%"
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                }

                MouseArea {
                    id: canvasMouse

                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true
                    focus: true

                    onPressed: function (mouse) {
                        canvas.forceActiveFocus();
                        var wx = canvas.toWorldX(mouse.x);
                        var wy = canvas.toWorldY(mouse.y);
                        var node = canvas.nodeAt(wx, wy);
                        canvas.lastMouseX = mouse.x;
                        canvas.lastMouseY = mouse.y;
                        canvas.pressX = mouse.x;
                        canvas.pressY = mouse.y;
                        canvas.pressNodeId = node === null ? -1 : node.id;
                        canvas.dragMoved = false;

                        if (mouse.button === Qt.RightButton) {
                            // 先在点击处做连线命中，菜单再就地弹出（坐标取画布内坐标）
                            canvas.contextEdgeId = canvas.edgeAt(mouse.x, mouse.y);
                            contextMenu.popup(mouse.x, mouse.y);
                            return;
                        }

                        if (node !== null) {
                            ruleGraph.selectNode(node.id);
                            // 输出端口：开始连线
                            if (Math.abs(wx - (node.x + node.width)) < canvas.portHitTolerance() && Math.abs(wy - (node.y + node.headerHeight / 2)) < canvas.portHitTolerance()) {
                                canvas.connectFrom = node.id;
                                canvas.tempX = wx;
                                canvas.tempY = wy;
                                return;
                            }
                            canvas.dragNodeId = node.id;
                            canvas.dragOriginX = wx;
                            canvas.dragOriginY = wy;
                            canvas.dragNodeX = node.x;
                            canvas.dragNodeY = node.y;
                            canvas.dragDx = 0;
                            canvas.dragDy = 0;
                            return;
                        }

                        canvas.panning = true;
                    }

                    onPositionChanged: function (mouse) {
                        // 位移未达阈值前不拖动、不拉线、不平移，松开仍算点击
                        if (!canvas.dragMoved) {
                            var dx = mouse.x - canvas.pressX;
                            var dy = mouse.y - canvas.pressY;
                            if (dx * dx + dy * dy < canvas.clickMoveThreshold * canvas.clickMoveThreshold) {
                                return;
                            }
                            canvas.dragMoved = true;
                        }
                        var wx = canvas.toWorldX(mouse.x);
                        var wy = canvas.toWorldY(mouse.y);
                        if (canvas.connectFrom > 0) {
                            canvas.tempX = wx;
                            canvas.tempY = wy;
                            return;
                        }
                        // 拖动只更新增量：代理按增量平移，节点坐标在松手时一次性写入模型
                        if (canvas.dragNodeId >= 0) {
                            canvas.dragDx = wx - canvas.dragOriginX;
                            canvas.dragDy = wy - canvas.dragOriginY;
                            return;
                        }
                        if (canvas.panning) {
                            canvas.panX += mouse.x - canvas.lastMouseX;
                            canvas.panY += mouse.y - canvas.lastMouseY;
                            canvas.lastMouseX = mouse.x;
                            canvas.lastMouseY = mouse.y;
                        }
                    }

                    onReleased: function (mouse) {
                        var wx = canvas.toWorldX(mouse.x);
                        var wy = canvas.toWorldY(mouse.y);
                        var dx = mouse.x - canvas.pressX;
                        var dy = mouse.y - canvas.pressY;
                        var moved = canvas.dragMoved || dx * dx + dy * dy >= canvas.clickMoveThreshold * canvas.clickMoveThreshold;
                        // 拖动落点在复位增量前算好：模型里仍是按下时的坐标，新坐标 = 旧坐标 + 增量
                        var dragId = canvas.dragNodeId;
                        var dragX = canvas.dragNodeX + canvas.dragDx;
                        var dragY = canvas.dragNodeY + canvas.dragDy;
                        var dragged = dragId >= 0 && canvas.dragMoved;
                        if (canvas.connectFrom > 0) {
                            // 原地松开只是点击输出端口，不产生连线
                            var port = moved ? canvas.inputPortAt(wx, wy) : null;
                            if (port !== null) {
                                ruleGraph.connectNodes(canvas.connectFrom, port.node, port.port);
                            }
                            // connectFrom 的哨兵是 -1，0 不是合法节点 id（桥接层 0 表示未选中）
                            canvas.connectFrom = -1;
                        } else if (!moved && mouse.button === Qt.LeftButton && canvas.pressNodeId > 0) {
                            canvas.handleNodeClick(canvas.pressNodeId, wx, wy);
                        }
                        // 复位与提交同处一个语句块：增量归零后到模型写回之间不会画出一帧原位
                        canvas.dragNodeId = -1;
                        canvas.dragDx = 0;
                        canvas.dragDy = 0;
                        canvas.panning = false;
                        canvas.pressNodeId = -1;
                        canvas.dragMoved = false;
                        if (dragged) {
                            ruleGraph.moveNode(dragId, dragX, dragY);
                        }
                    }

                    onWheel: function (wheel) {
                        var factor = wheel.angleDelta.y > 0 ? 1.1 : 0.9;
                        var newZoom = Math.max(ComponentStyle.graphZoomMin, Math.min(ComponentStyle.graphZoomMax, canvas.zoom * factor));
                        // 以鼠标位置为锚点缩放
                        var wx = canvas.toWorldX(wheel.x);
                        var wy = canvas.toWorldY(wheel.y);
                        canvas.zoom = newZoom;
                        canvas.panX = wheel.x - wx * newZoom;
                        canvas.panY = wheel.y - wy * newZoom;
                    }

                    Keys.onPressed: function (event) {
                        if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
                            if (ruleGraph.selectedNodeId > 0 && ruleGraph.selectedNode.type !== "defaultOutput") {
                                ruleGraph.deleteNode(ruleGraph.selectedNodeId);
                            }
                            event.accepted = true;
                        } else if (event.modifiers & Qt.ControlModifier && event.key === Qt.Key_C) {
                            ruleGraph.copySelection();
                            event.accepted = true;
                        } else if (event.modifiers & Qt.ControlModifier && event.key === Qt.Key_V) {
                            ruleGraph.pasteClipboard(30, 30);
                            event.accepted = true;
                        }
                    }
                }

                AppMenu {
                    id: contextMenu

                    MenuItem {
                        text: qsTr("添加规则节点")
                        onTriggered: ruleGraph.addRuleNode(canvas.toWorldX(canvas.lastMouseX) - 80, canvas.toWorldY(canvas.lastMouseY) - 20)
                    }
                    MenuItem {
                        text: qsTr("添加加法节点")
                        onTriggered: ruleGraph.addOperatorNode("+", canvas.toWorldX(canvas.lastMouseX), canvas.toWorldY(canvas.lastMouseY))
                    }
                    MenuItem {
                        text: qsTr("添加开根号节点")
                        onTriggered: ruleGraph.addAdvancedNode("sqrt", canvas.toWorldX(canvas.lastMouseX), canvas.toWorldY(canvas.lastMouseY))
                    }
                    MenuItem {
                        text: qsTr("添加自定义表达式")
                        onTriggered: ruleGraph.addAdvancedNode("expression", canvas.toWorldX(canvas.lastMouseX), canvas.toWorldY(canvas.lastMouseY))
                    }
                    MenuSeparator {}
                    // 仅右键命中连线时生成该项，删除走 C++ 的 ruleGraph.deleteEdge
                    Repeater {
                        model: canvas.contextEdgeId > 0 ? 1 : 0
                        MenuItem {
                            objectName: "ruleDeleteEdgeMenu"
                            text: qsTr("删除此连线")
                            onTriggered: ruleGraph.deleteEdge(canvas.contextEdgeId)
                        }
                    }
                    MenuItem {
                        text: qsTr("删除选中节点")
                        enabled: ruleGraph.selectedNodeId > 0 && ruleGraph.selectedNode.type !== "defaultOutput"
                        onTriggered: ruleGraph.deleteNode(ruleGraph.selectedNodeId)
                    }
                }
            }
        }

        // 检查器
        ColumnLayout {
            id: rightCol
            SplitView.preferredWidth: ComponentStyle.dialogSidePanelWidth
            SplitView.minimumWidth: 220
            Layout.fillHeight: true
            spacing: Metrics.spacingMd

            Text {
                text: qsTr("属性")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
            }

            Text {
                text: ruleGraph.selectedNode.name === undefined ? qsTr("未选中节点") : ruleGraph.selectedNode.name
                color: Theme.textPrimary
                font.pixelSize: Typography.fontBody
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            // 属性面板按节点类型只构建适用的控件：不适用的组根本不创建，而不是置灰
            Loader {
                Layout.fillWidth: true
                active: ruleGraph.selectedNode.type === "rule"
                sourceComponent: ruleFields
            }

            Loader {
                Layout.fillWidth: true
                active: ruleGraph.selectedNode.type === "rule" || ruleGraph.selectedNode.advanced === "expression"
                sourceComponent: expressionFields
            }

            Loader {
                Layout.fillWidth: true
                active: ruleGraph.selectedNode.type === "channel"
                sourceComponent: channelFields
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("提示：拖拽节点移动（单击不移动则进入插件图），从右侧圆点拖到左侧端口连线；右键画布添加节点；Ctrl+C/V、Delete 操作选中节点。")
                color: Theme.textMuted
                font.pixelSize: Typography.fontBody
                wrapMode: Text.WordWrap
            }

            AppButton {
                objectName: "ruleFitViewButton"
                Layout.fillWidth: true
                text: qsTr("适配视图")
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }

    // 规则节点专用：规则名 / 值模式 / 启用
    Component {
        id: ruleFields

        ColumnLayout {
            spacing: Metrics.spacingMd

            AppTextField {
                id: nameField

                Layout.fillWidth: true
                placeholderText: qsTr("规则名")
                text: ruleGraph.selectedNode.ruleName === undefined ? "" : ruleGraph.selectedNode.ruleName
                onEditingFinished: ruleGraph.renameRuleNode(ruleGraph.selectedNode.id, text)
            }

            AppComboBox {
                id: modeCombo

                Layout.fillWidth: true
                model: [qsTr("递减"), qsTr("递增"), qsTr("设为"), qsTr("连减"), qsTr("连增")]
                currentIndex: ruleGraph.selectedNode.mode === undefined ? 1 : ruleGraph.selectedNode.mode
                onActivated: ruleGraph.setRuleMode(ruleGraph.selectedNode.id, currentIndex)
            }

            AppCheckBox {
                text: qsTr("启用规则")
                checked: ruleGraph.selectedNode.enabled === true
                onClicked: ruleGraph.setRuleEnabled(ruleGraph.selectedNode.id, checked)
            }
        }
    }

    // 规则节点与自定义表达式节点共用：值模式 / 表达式
    Component {
        id: expressionFields

        ColumnLayout {
            spacing: Metrics.spacingMd

            Text {
                text: qsTr("值模式 / 表达式")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
            }

            AppTextArea {
                id: expressionArea

                Layout.fillWidth: true
                Layout.preferredHeight: 90
                wrapMode: TextArea.Wrap
                font.family: Typography.fontMonoFamily
                font.pixelSize: Typography.fontBody
                text: ruleGraph.selectedNode.expression === undefined ? "" : ruleGraph.selectedNode.expression
                onEditingFinished: ruleGraph.setRuleExpression(ruleGraph.selectedNode.id, text)
            }

            AppButton {
                Layout.fillWidth: true
                text: qsTr("应用表达式")
                onClicked: ruleGraph.setRuleExpression(ruleGraph.selectedNode.id, expressionArea.text)
            }
        }
    }

    // 通道节点专用：目标设备（跟随默认设备 / 在线输出设备 / 已记忆设备，离线项标注「离线」）
    Component {
        id: channelFields

        ColumnLayout {
            spacing: Metrics.spacingMd

            Text {
                Layout.fillWidth: true
                text: qsTr("目标设备")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontBody
            }

            AppComboBox {
                objectName: "ruleChannelDeviceCombo"
                Layout.fillWidth: true
                model: ruleGraph.channelDeviceOptions
                textRole: "name"
                currentIndex: ruleGraph.channelDeviceIndex(ruleGraph.selectedNode.targetDevice === undefined ? "" : ruleGraph.selectedNode.targetDevice)
                onActivated: ruleGraph.setChannelDeviceTargetByIndex(ruleGraph.selectedNode.id, currentIndex)
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("通道节点是输出端点：把上游节点的输出端口拖到它的输入端口，求值结果按上面选中的设备下发。同一设备同一通道只保留一条入边，连入新连线会顶掉原连线。")
                color: Theme.textMuted
                font.pixelSize: Typography.fontBody
                wrapMode: Text.WordWrap
            }
        }
    }

    // 底部按钮区：左次要（关闭）、右主操作（保存规则图）
    RowLayout {
        id: ruleFooterRow

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Metrics.spacingMd
        spacing: Metrics.spacingMd

        AppButton {
            objectName: "ruleEditorCloseButton"
            text: qsTr("关闭")
        }

        Item {
            Layout.fillWidth: true
        }

        // 一次保存写全部有改动的图（总图 + 插件图），按钮上给出待写入的张数
        AppButton {
            objectName: "ruleSaveButton"
            text: ruleGraph.dirty ? qsTr("保存全部（%1 张图） *").arg(ruleGraph.dirtyGraphs.length) : qsTr("保存规则图")
            primary: true
        }
    }

    // 未保存改动的三选确认：声明在编辑器窗口内部，弹层因此附着在编辑器的 Overlay 上，
    // 显示在编辑器之上（主窗口保持可用，编辑器不做 ApplicationModal）
    RuleDiscardDialog {
        id: ruleDiscardDialog
    }
}
