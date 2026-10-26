pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes

// 强度曲线预览：淡黄描边 + 向底色收拢的渐变填充，含 100/75/50/25 四条相对网格线，
// showAxisLabels 打开时在右侧显示刻度。points 与 frames 取先有值的那个序列。
// 用法：AppSparkline { points: [0, 20, 60, 100]; maxValue: 100; showAxisLabels: true }

Item {
    id: root

    property var points: []
    property var frames: []
    property real maxValue: 100
    property bool showAxisLabels: false

    readonly property var series: (root.points && root.points.length > 0) ? root.points : (root.frames ? root.frames : [])
    readonly property int count: root.series.length
    readonly property var gridLevels: [1.0, 0.75, 0.5, 0.25]
    readonly property real plotInset: ComponentStyle.waveInset * 2
    readonly property real axisLabelWidth: 28
    readonly property real fillOpacity: 0.28
    readonly property real plotLeft: root.plotInset
    readonly property real plotRight: root.showAxisLabels ? root.width - root.plotInset - root.axisLabelWidth : root.width - root.plotInset
    readonly property real plotTop: root.plotInset
    readonly property real plotBottom: root.height - root.plotInset

    implicitWidth: ComponentStyle.wavePreviewMinWidth
    implicitHeight: ComponentStyle.wavePreviewHeight
    opacity: root.enabled ? 1.0 : Metrics.disabledOpacity
    clip: true

    function valueAt(index) {
        var raw = root.maxValue > 0 ? root.series[index] / root.maxValue : 0;
        return Math.max(0.0, Math.min(1.0, raw));
    }

    function xAt(index) {
        var span = root.plotRight - root.plotLeft;
        return root.plotLeft + (root.count > 1 ? index / (root.count - 1) : 0) * span;
    }

    function yAt(index) {
        return root.plotBottom - root.valueAt(index) * (root.plotBottom - root.plotTop);
    }

    function levelY(level) {
        return root.plotBottom - level * (root.plotBottom - root.plotTop);
    }

    function buildStrokePath() {
        if (root.count === 0) {
            return "";
        }
        var path = "M " + root.xAt(0).toFixed(2) + " " + root.yAt(0).toFixed(2);
        for (var i = 1; i < root.count; i++) {
            path += " L " + root.xAt(i).toFixed(2) + " " + root.yAt(i).toFixed(2);
        }
        return path;
    }

    function buildFillPath() {
        if (root.count === 0) {
            return "";
        }
        return root.buildStrokePath()
            + " L " + root.xAt(root.count - 1).toFixed(2) + " " + root.plotBottom.toFixed(2)
            + " L " + root.xAt(0).toFixed(2) + " " + root.plotBottom.toFixed(2) + " Z";
    }

    Rectangle {
        anchors.fill: parent
        radius: Metrics.radiusSm
        color: Theme.surfaceAlt
        border.width: Metrics.borderWidth
        border.color: (!root.enabled) ? Theme.borderDisabled : (hoverHandler.hovered ? Theme.border : Theme.divider)
    }

    HoverHandler {
        id: hoverHandler
    }

    Repeater {
        model: root.gridLevels

        delegate: Rectangle {
            required property real modelData

            x: root.plotLeft
            y: root.levelY(modelData)
            width: Math.max(0, root.plotRight - root.plotLeft)
            height: Metrics.borderWidth
            color: Theme.divider
        }
    }

    Shape {
        anchors.fill: parent
        visible: root.count > 0
        opacity: root.enabled ? root.fillOpacity : 0.0

        ShapePath {
            strokeColor: Theme.transparentFill
            strokeWidth: 0
            fillColor: Theme.transparentFill
            fillGradient: LinearGradient {
                x1: 0
                y1: root.plotTop
                x2: 0
                y2: root.plotBottom

                GradientStop { position: 0.0; color: Theme.accent }
                GradientStop { position: 1.0; color: Theme.surfaceAlt }
            }

            PathSvg {
                path: root.buildFillPath()
            }
        }
    }

    Shape {
        anchors.fill: parent
        visible: root.enabled && root.count > 0

        ShapePath {
            strokeColor: Theme.accent
            strokeWidth: ComponentStyle.waveStrokeWidth
            fillColor: Theme.transparentFill
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: root.buildStrokePath()
            }
        }
    }

    Repeater {
        model: root.gridLevels

        delegate: Text {
            required property real modelData

            visible: root.showAxisLabels
            x: root.plotRight + Metrics.spacingXs
            y: root.levelY(modelData) - height / 2
            text: Math.round(modelData * 100).toString()
            color: Theme.textMuted
            font.pixelSize: Typography.fontBody
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.count === 0
        text: qsTr("无数据")
        color: Theme.textMuted
        font.pixelSize: Typography.fontBody
    }
}
