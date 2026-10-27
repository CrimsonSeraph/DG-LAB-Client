import QtQuick
import QtQuick.Layouts

// 日志页：运行日志列表、级别过滤与手动导出选项（导出级别 / 导出范围 / 导出目录）。
// 页面以 ColumnLayout 撑满窗口高度，日志列表吃满面板剩余高度并自行滚动。
// 交互约定：本文件不写任何信号处理器；交互控件由 C++（UiConnector）按 objectName 连接。
Item {
    id: page

    objectName: "logPage"

    // 级别名称：副标题用，顺序与 DebugLog 的 LogLevel 取值一致
    readonly property var levelNames: ["DEBUG", "INFO", "WARN", "ERROR", "NONE"]
    // 下拉模型用 { text } 对象并配 textRole：AppComboBox 的委托只在 textRole 非空时取模型字段
    readonly property var levelModel: [{ text: "DEBUG" }, { text: "INFO" }, { text: "WARN" }, { text: "ERROR" }]
    readonly property var rangeModel: [
        { text: qsTr("仅该级别") },
        { text: qsTr("该级别及以上") },
        { text: qsTr("该级别及以下") }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin
        spacing: Metrics.cardSpacing

        PageHeader {
            Layout.fillWidth: true
            title: qsTr("日志")
            subtitle: qsTr("运行日志 %1 条 · 界面级别 %2")
                .arg(logBridge.messages.length)
                .arg(page.levelNames[logBridge.levelFilter])
        }

        // 日志面板：撑满页头以下的高度，最小高度取 ComponentStyle.logCardHeight 令牌
        Rectangle {
            objectName: "logPanel"

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: ComponentStyle.logCardHeight
            radius: Metrics.cardRadius
            color: Theme.surface
            border.width: Metrics.borderWidth
            border.color: Theme.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Metrics.cardPadding
                spacing: ComponentStyle.groupTitleGap

                // 卡头行：标题与级别过滤 / 导出 / 清空同一行
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingMd

                    Text {
                        text: qsTr("运行日志")
                        color: Theme.textPrimary
                        font.pixelSize: Typography.fontSubtitle
                        font.bold: true
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Text {
                        text: qsTr("级别")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                    }

                    AppComboBox {
                        objectName: "logLevelCombo"
                        Layout.preferredWidth: ComponentStyle.inputMinWidth
                        model: page.levelModel
                        textRole: "text"
                        currentIndex: logBridge.levelFilter
                    }

                    AppButton {
                        objectName: "logExportButton"
                        text: qsTr("导出日志")
                    }

                    AppButton {
                        objectName: "logClearButton"
                        text: qsTr("清空")
                    }
                }

                // 导出选项：写入 user.json 的 app.log.manual.*（导出时按这三项逐行过滤）
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingMd

                    Text {
                        text: qsTr("导出级别")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                    }

                    AppComboBox {
                        objectName: "logExportLevelCombo"
                        Layout.preferredWidth: ComponentStyle.inputMinWidth
                        model: page.levelModel
                        textRole: "text"
                        currentIndex: logBridge.exportLevel
                    }

                    Text {
                        text: qsTr("导出范围")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                    }

                    AppComboBox {
                        objectName: "logExportRangeCombo"
                        Layout.preferredWidth: ComponentStyle.inputMinWidth
                        model: page.rangeModel
                        textRole: "text"
                        currentIndex: logBridge.exportRange
                    }

                    Text {
                        text: qsTr("导出目录")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                    }

                    AppTextField {
                        objectName: "logExportDirField"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 0
                        text: logBridge.exportDirConfig
                        placeholderText: qsTr("相对程序目录或绝对路径")
                    }

                    AppButton {
                        objectName: "logExportDirApplyButton"
                        text: qsTr("应用目录")
                    }
                }

                // 贴底跟随：stickToBottom 由 C++ 侧（UiConnector）按用户滚动结果维护，
                // 绑定只在需要跟随时生效，用户上滚回看历史日志时不再被新日志拽回底部。
                ListView {
                    id: logList

                    objectName: "logList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: ComponentStyle.logListMinHeight
                    clip: true
                    model: logBridge.messages
                    spacing: 0
                    property bool stickToBottom: true

                    delegate: Rectangle {
                        width: logList.width
                        height: ComponentStyle.logRowHeight
                        color: Theme.transparentFill

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            text: modelData.text
                            color: modelData.level >= 3 ? Theme.danger : (modelData.level === 2 ? Theme.warning : Theme.textSecondary)
                            font.family: Typography.fontMonoFamily
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }
                    }

                    // 无信号处理器：新日志到达时内容高度变化，绑定自动滚到底部；
                    // stickToBottom 为 false 时绑定不生效（restoreMode 不还原），用户停留原处
                    Binding {
                        target: logList
                        property: "contentY"
                        value: Math.max(0, logList.contentHeight - logList.height)
                        when: logList.stickToBottom
                        restoreMode: Binding.RestoreNone
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("导出目录：%1 · 相对路径基于程序目录解析").arg(logBridge.exportDir)
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                    elide: Text.ElideMiddle
                }
            }
        }
    }
}
