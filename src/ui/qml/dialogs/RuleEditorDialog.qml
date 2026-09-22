import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// 规则可视化编辑器（ComfyUI / UE 蓝图风格）。
//
// 信号处理器例外：本对话框的画布与节点手势（拖动、滚轮缩放、右键菜单、快捷键）
// 属于 src/ui/README.md 中登记的"必须使用手势的视图"，允许写处理器；处理器只调用
// ruleGraph（C++）的方法，图的增删改/复制粘贴/写回全部在 C++ 完成。
Dialog {
    id: dialog

    objectName: "ruleEditorDialog"
    title: qsTr("规则编辑器")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 1100
    height: 700
    padding: Metrics.spacingMd

    background: Rectangle {
        radius: Metrics.radiusMd
        color: Theme.surface
        border.width: Metrics.borderWidth
        border.color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        spacing: Metrics.spacingLg

        // ------------------------------------------------------ 模块 / 节点面板
        ColumnLayout {
            Layout.preferredWidth: 210
            Layout.fillHeight: true
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
                    height: 30
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
                        font.pixelSize: Typography.fontCaption
                        elide: Text.ElideRight
                        width: parent.width - 12
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: ruleGraph.addModuleNode(modelData.valueId, 60, 80 + moduleList.contentY / 2)
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

                AppButton { objectName: "ruleAddAdd"; text: "+"; onClicked: ruleGraph.addOperatorNode("+", 300, 120) }
                AppButton { objectName: "ruleAddSub"; text: "−"; onClicked: ruleGraph.addOperatorNode("-", 300, 120) }
                AppButton { objectName: "ruleAddMul"; text: "×"; onClicked: ruleGraph.addOperatorNode("*", 300, 120) }
                AppButton { objectName: "ruleAddDiv"; text: "÷"; onClicked: ruleGraph.addOperatorNode("/", 300, 120) }
                AppButton { objectName: "ruleAddAbs"; text: qsTr("绝对值"); onClicked: ruleGraph.addAdvancedNode("abs", 300, 260) }
                AppButton { objectName: "ruleAddSquare"; text: qsTr("平方"); onClicked: ruleGraph.addAdvancedNode("square", 300, 260) }
                AppButton { objectName: "ruleAddSqrt"; text: qsTr("开根号"); onClicked: ruleGraph.addAdvancedNode("sqrt", 300, 260) }
                AppButton { objectName: "ruleAddExpr"; text: qsTr("表达式"); onClicked: ruleGraph.addAdvancedNode("expression", 300, 260) }
                AppButton { objectName: "ruleAddRule"; text: qsTr("规则节点"); onClicked: ruleGraph.addRuleNode(520, 120) }
                AppButton { objectName: "ruleAddChannelA"; text: qsTr("通道 A"); onClicked: ruleGraph.addChannelNode("A", 820, 80) }
                AppButton { objectName: "ruleAddChannelB"; text: qsTr("通道 B"); onClicked: ruleGraph.addChannelNode("B", 820, 260) }
            }

            Item {
                Layout.fillHeight: true
            }
        }

        // ------------------------------------------------------ 画布
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface
            radius: Metrics.radiusMd
            border.width: Metrics.borderWidth
            border.color: Theme.border
            clip: true

            Item {
                id: canvas

                anchors.fill: parent

                property real panX: 40
                property real panY: 20
                property real zoom: 1.0
                property int dragNodeId: 0
                property real dragOffsetX: 0
                property real dragOffsetY: 0
                property int connectFrom: 0
                property real tempX: 0
                property real tempY: 0
                property bool panning: false
                property real lastMouseX: 0
                property real lastMouseY: 0

                function toWorldX(x) {
                    return (x - panX) / zoom;
                }

                function toWorldY(y) {
                    return (y - panY) / zoom;
                }

                function nodeAt(wx, wy) {
                    var list = ruleGraph.nodes;
                    for (var i = list.length - 1; i >= 0; i--) {
                        var n = list[i];
                        var height = n.headerHeight + n.portInset + Math.max(1, n.inputCount) * n.portSpacing + 12;
                        if (wx >= n.x && wx <= n.x + n.width && wy >= n.y && wy <= n.y + height) {
                            return n;
                        }
                    }
                    return null;
                }

                function inputPortAt(wx, wy) {
                    var list = ruleGraph.nodes;
                    for (var i = 0; i < list.length; i++) {
                        var n = list[i];
                        for (var p = 0; p < n.inputCount; p++) {
                            var px = n.x;
                            var py = n.y + n.headerHeight + n.portInset + p * n.portSpacing;
                            if (Math.abs(wx - px) < 12 && Math.abs(wy - py) < 10) {
                                return {
                                    "node": n.id,
                                    "port": p
                                };
                            }
                        }
                    }
                    return null;
                }

                Item {
                    id: world

                    x: canvas.panX
                    y: canvas.panY
                    width: 4000
                    height: 3000
                    transform: Scale {
                        origin.x: 0
                        origin.y: 0
                        xScale: canvas.zoom
                        yScale: canvas.zoom
                    }

                    // 网格
                    Repeater {
                        model: 120

                        delegate: Rectangle {
                            x: 0
                            y: index * 40
                            width: 4000
                            height: 1
                            color: Theme.divider
                            opacity: 0.4
                        }
                    }

                    // 连线
                    Repeater {
                        model: ruleGraph.edges

                        delegate: Shape {
                            anchors.fill: parent
                            antialiasing: true

                            ShapePath {
                                strokeColor: Theme.accent
                                strokeWidth: 2
                                fillColor: "transparent"
                                startX: modelData.fromX
                                startY: modelData.fromY
                                PathCubic {
                                    control1X: modelData.fromX + 60
                                    control1Y: modelData.fromY
                                    control2X: modelData.toX - 60
                                    control2Y: modelData.toY
                                    x: modelData.toX
                                    y: modelData.toY
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
                            strokeWidth: 2
                            strokeStyle: ShapePath.DashLine
                            fillColor: "transparent"
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

                            x: modelData.x
                            y: modelData.y
                            width: modelData.width
                            height: modelData.headerHeight + modelData.portInset + Math.max(1, modelData.inputCount) * modelData.portSpacing + 12
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
                                    width: parent.width - 12
                                    text: modelData.name
                                    color: Theme.textPrimary
                                    font.pixelSize: Typography.fontCaption
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                            }

                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.topMargin: modelData.headerHeight + 6
                                spacing: modelData.portSpacing - Typography.fontCaption - 4

                                Repeater {
                                    model: modelData.inputCount

                                    delegate: Row {
                                        height: Typography.fontCaption + 4

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
                                            font.pixelSize: Typography.fontCaption
                                        }
                                    }
                                }
                            }

                            // 输出端口
                            Rectangle {
                                width: 12
                                height: 12
                                radius: 6
                                color: Theme.accent
                                x: parent.width - 6
                                y: modelData.headerHeight / 2 - 6
                            }
                        }
                    }
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
                        canvas.lastMouseX = mouse.x;
                        canvas.lastMouseY = mouse.y;

                        if (mouse.button === Qt.RightButton) {
                            contextMenu.popup();
                            return;
                        }

                        var node = canvas.nodeAt(wx, wy);
                        if (node !== null) {
                            ruleGraph.selectNode(node.id);
                            // 输出端口：开始连线
                            if (Math.abs(wx - (node.x + node.width)) < 14 && Math.abs(wy - (node.y + node.headerHeight / 2)) < 14) {
                                canvas.connectFrom = node.id;
                                canvas.tempX = wx;
                                canvas.tempY = wy;
                                return;
                            }
                            canvas.dragNodeId = node.id;
                            canvas.dragOffsetX = wx - node.x;
                            canvas.dragOffsetY = wy - node.y;
                            return;
                        }

                        canvas.panning = true;
                    }

                    onPositionChanged: function (mouse) {
                        var wx = canvas.toWorldX(mouse.x);
                        var wy = canvas.toWorldY(mouse.y);
                        if (canvas.connectFrom > 0) {
                            canvas.tempX = wx;
                            canvas.tempY = wy;
                            return;
                        }
                        if (canvas.dragNodeId > 0) {
                            ruleGraph.moveNode(canvas.dragNodeId, wx - canvas.dragOffsetX, wy - canvas.dragOffsetY);
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
                        if (canvas.connectFrom > 0) {
                            var port = canvas.inputPortAt(wx, wy);
                            if (port !== null) {
                                ruleGraph.connectNodes(canvas.connectFrom, port.node, port.port);
                            }
                            canvas.connectFrom = 0;
                        }
                        canvas.dragNodeId = 0;
                        canvas.panning = false;
                    }

                    onWheel: function (wheel) {
                        var factor = wheel.angleDelta.y > 0 ? 1.1 : 0.9;
                        var newZoom = Math.max(0.4, Math.min(2.0, canvas.zoom * factor));
                        // 以鼠标位置为锚点缩放
                        var wx = canvas.toWorldX(wheel.x);
                        var wy = canvas.toWorldY(wheel.y);
                        canvas.zoom = newZoom;
                        canvas.panX = wheel.x - wx * newZoom;
                        canvas.panY = wheel.y - wy * newZoom;
                    }

                    Keys.onPressed: function (event) {
                        if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
                            if (ruleGraph.selectedNodeId > 0) {
                                ruleGraph.deleteNode(ruleGraph.selectedNodeId);
                            }
                            event.accepted = true;
                        }
                        else if (event.modifiers & Qt.ControlModifier && event.key === Qt.Key_C) {
                            ruleGraph.copySelection();
                            event.accepted = true;
                        }
                        else if (event.modifiers & Qt.ControlModifier && event.key === Qt.Key_V) {
                            ruleGraph.pasteClipboard(30, 30);
                            event.accepted = true;
                        }
                    }
                }

                Menu {
                    id: contextMenu

                    MenuItem { text: qsTr("添加规则节点"); onTriggered: ruleGraph.addRuleNode(canvas.toWorldX(canvas.lastMouseX) - 80, canvas.toWorldY(canvas.lastMouseY) - 20) }
                    MenuItem { text: qsTr("添加加法节点"); onTriggered: ruleGraph.addOperatorNode("+", canvas.toWorldX(canvas.lastMouseX), canvas.toWorldY(canvas.lastMouseY)) }
                    MenuItem { text: qsTr("添加开根号节点"); onTriggered: ruleGraph.addAdvancedNode("sqrt", canvas.toWorldX(canvas.lastMouseX), canvas.toWorldY(canvas.lastMouseY)) }
                    MenuItem { text: qsTr("添加自定义表达式"); onTriggered: ruleGraph.addAdvancedNode("expression", canvas.toWorldX(canvas.lastMouseX), canvas.toWorldY(canvas.lastMouseY)) }
                    MenuSeparator {}
                    MenuItem { text: qsTr("删除选中节点"); enabled: ruleGraph.selectedNodeId > 0; onTriggered: ruleGraph.deleteNode(ruleGraph.selectedNodeId) }
                }
            }
        }

        // ------------------------------------------------------ 检查器
        ColumnLayout {
            Layout.preferredWidth: 250
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

            AppTextField {
                id: nameField

                placeholderText: qsTr("规则名")
                text: ruleGraph.selectedNode.type === "rule" ? ruleGraph.selectedNode.ruleName : ""
                Layout.fillWidth: true
                enabled: ruleGraph.selectedNode.type === "rule"
                onEditingFinished: if (ruleGraph.selectedNode.type === "rule") ruleGraph.renameRuleNode(ruleGraph.selectedNode.id, text)
            }

            AppComboBox {
                id: modeCombo

                Layout.fillWidth: true
                enabled: ruleGraph.selectedNode.type === "rule"
                model: [qsTr("递减"), qsTr("递增"), qsTr("设为"), qsTr("连减"), qsTr("连增")]
                currentIndex: ruleGraph.selectedNode.mode === undefined ? 1 : ruleGraph.selectedNode.mode
                onActivated: ruleGraph.setRuleMode(ruleGraph.selectedNode.id, currentIndex)
            }

            AppCheckBox {
                text: qsTr("启用规则")
                enabled: ruleGraph.selectedNode.type === "rule"
                checked: ruleGraph.selectedNode.enabled === true
                onClicked: ruleGraph.setRuleEnabled(ruleGraph.selectedNode.id, checked)
            }

            Text {
                text: qsTr("值模式 / 表达式")
                color: Theme.textSecondary
                font.pixelSize: Typography.fontCaption
            }

            AppTextArea {
                id: expressionArea

                Layout.fillWidth: true
                Layout.preferredHeight: 90
                wrapMode: TextArea.Wrap
                font.family: "Consolas"
                font.pixelSize: Typography.fontSmall
                text: ruleGraph.selectedNode.expression === undefined ? "" : ruleGraph.selectedNode.expression
                enabled: ruleGraph.selectedNode.type === "rule" || ruleGraph.selectedNode.advanced === "expression"
                onEditingFinished: ruleGraph.setRuleExpression(ruleGraph.selectedNode.id, text)
            }

            AppButton {
                Layout.fillWidth: true
                text: qsTr("应用表达式")
                onClicked: ruleGraph.setRuleExpression(ruleGraph.selectedNode.id, expressionArea.text)
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("提示：拖拽节点移动，从右侧圆点拖到左侧端口连线；右键画布添加节点；Ctrl+C/V、Delete 操作选中节点。")
                color: Theme.textMuted
                font.pixelSize: Typography.fontCaption
                wrapMode: Text.WordWrap
            }

            Item {
                Layout.fillHeight: true
            }

            AppButton {
                objectName: "ruleSaveButton"
                Layout.fillWidth: true
                text: ruleGraph.dirty ? qsTr("保存规则图 *") : qsTr("保存规则图")
                primary: true
                onClicked: ruleGraph.save()
            }

            AppButton {
                objectName: "ruleEditorCloseButton"
                Layout.fillWidth: true
                text: qsTr("关闭")
                onClicked: dialog.close()
            }
        }
    }
}
