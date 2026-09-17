import QtQuick
import QtQuick.Shapes

// 波形预览：强度曲线（主色）与频率曲线（副色）。
// 使用 Shape/PathPolyline（保留模式），不写任何信号处理器。
Item {
    id: root

    property var points: []
    property bool showFrequency: true

    implicitWidth: ComponentStyle.wavePreviewMinWidth
    implicitHeight: ComponentStyle.wavePreviewHeight
    clip: true

    Rectangle {
        anchors.fill: parent
        radius: Metrics.radiusSm
        color: Theme.surface
        border.width: Metrics.borderWidth
        border.color: Theme.divider
    }

    Shape {
        anchors.fill: parent
        anchors.margins: 3

        ShapePath {
            strokeColor: Theme.accent
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathPolyline {
                path: root.buildPath(false)
            }
        }
    }

    Shape {
        anchors.fill: parent
        anchors.margins: 3
        visible: root.showFrequency && root.points.length > 0

        ShapePath {
            strokeColor: Theme.secondary
            strokeWidth: 1
            fillColor: "transparent"
            PathPolyline {
                path: root.buildPath(true)
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.points.length === 0
        text: qsTr("无波形")
        color: Theme.textMuted
        font.pixelSize: Typography.fontCaption
    }

    /// @brief 生成曲线路径；freq 为 true 时生成频率曲线（0~240 归一化）
    function buildPath(freq) {
        var result = [];
        var pts = root.points;
        if (!pts || pts.length === 0) {
            return result;
        }
        var w = root.width - 6;
        var h = root.height - 6;
        if (w <= 0 || h <= 0) {
            return result;
        }
        var count = pts.length;
        for (var i = 0; i < count; i++) {
            var x = count > 1 ? (i / (count - 1)) * w : 0;
            var raw = freq ? pts[i].freq / 240.0 : pts[i].strength / 100.0;
            var value = Math.max(0.0, Math.min(1.0, raw));
            result.push(Qt.point(x, h - value * h));
        }
        return result;
    }
}
