pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

// 插件中心：两列卡片网格 + 右侧已启用栏，点卡片或「配置」进入同页详情。
// 卡片状态与已启用栏都读 moduleBridge.modules，启停后由桥接刷新驱动。
Item {
    id: page

    objectName: "pluginCenterPage"

    property string searchText: ""
    property int categoryIndex: 0
    property int viewMode: 0
    property string selectedFile: ""
    property bool detailVisible: false

    // 分类判定：游戏 = 标签含「游戏」或「HTTP」；外设 = 标签含「WebSocket」「桥接」「串口」「HID」「外设」
    readonly property var categoryOptions: [qsTr("全部"), qsTr("游戏"), qsTr("外设")]
    readonly property int sidebarWidth: Metrics.navWidth + Metrics.spacing3xl * 2

    function pluginList() {
        const source = moduleBridge.modules;
        const list = [];
        for (let i = 0; i < source.length; ++i) {
            if (String(source[i].kind) === "plugin") {
                list.push(source[i]);
            }
        }
        return list;
    }

    function tagList(item) {
        return (item.tags === undefined || item.tags === null) ? [] : item.tags;
    }

    function matchCategory(item) {
        if (page.categoryIndex === 0) {
            return true;
        }
        const keywords = page.categoryIndex === 1
            ? ["游戏", "http"]
            : ["websocket", "桥接", "串口", "hid", "外设"];
        const tags = page.tagList(item);
        for (let t = 0; t < tags.length; ++t) {
            const value = String(tags[t]).toLowerCase();
            for (let k = 0; k < keywords.length; ++k) {
                if (value.indexOf(keywords[k]) >= 0) {
                    return true;
                }
            }
        }
        return false;
    }

    function matchSearch(item) {
        const needle = page.searchText.toLowerCase();
        if (needle.length === 0) {
            return true;
        }
        const fields = [item.name, item.author, item.moduleName];
        const tags = page.tagList(item);
        for (let t = 0; t < tags.length; ++t) {
            fields.push(tags[t]);
        }
        for (let f = 0; f < fields.length; ++f) {
            const value = (fields[f] === undefined || fields[f] === null) ? "" : String(fields[f]);
            if (value.toLowerCase().indexOf(needle) >= 0) {
                return true;
            }
        }
        return false;
    }

    function visiblePlugins() {
        const all = page.pluginList();
        const list = [];
        for (let i = 0; i < all.length; ++i) {
            if (page.matchSearch(all[i]) && page.matchCategory(all[i])) {
                list.push(all[i]);
            }
        }
        return list;
    }

    function enabledPlugins() {
        const all = page.pluginList();
        const list = [];
        for (let i = 0; i < all.length; ++i) {
            if (all[i].enabled === true) {
                list.push(all[i]);
            }
        }
        return list;
    }

    function findPlugin(fileName) {
        const all = page.pluginList();
        for (let i = 0; i < all.length; ++i) {
            if (String(all[i].fileName) === String(fileName)) {
                return all[i];
            }
        }
        return null;
    }

    function eventsFor(moduleName) {
        const source = eventBridge.events;
        for (let i = 0; i < source.length; ++i) {
            if (String(source[i].module) === String(moduleName)) {
                return source[i];
            }
        }
        return null;
    }

    function openDetail(fileName) {
        page.selectedFile = fileName;
        page.detailVisible = true;
    }

    readonly property var selectedPlugin: page.findPlugin(page.selectedFile)
    readonly property var selectedEvents: page.selectedPlugin === null
        ? null
        : page.eventsFor(String(page.selectedPlugin.moduleName))

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin
        spacing: Metrics.cardSpacing

        PageHeader {
            Layout.fillWidth: true
            title: qsTr("插件中心")
            subtitle: page.detailVisible
                ? qsTr("插件详情与事件映射")
                : qsTr("%1 个插件 · %2 个已启用").arg(page.pluginList().length).arg(page.enabledPlugins().length)
        }

        // 查询周期：原模块页的统一周期控件，objectName 不变，UiConnector 的既有连接继续生效
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: periodRow.implicitHeight + Metrics.cardPadding * 2
            radius: Metrics.cardRadius
            color: Theme.surface
            border.width: Metrics.borderWidth
            border.color: Theme.border

            RowLayout {
                id: periodRow

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: Metrics.cardPadding
                spacing: Metrics.spacingMd

                Text {
                    text: qsTr("统一查询周期")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                }

                AppComboBox {
                    id: periodCombo

                    objectName: "modulePeriodCombo"
                    model: moduleBridge.periodOptions
                    textRole: "text"
                    Layout.preferredWidth: 160
                    // 回显实际周期：全部数值一致时选中该周期，不一致时无选中项
                    Binding on currentIndex {
                        value: moduleBridge.uniformPeriodIndex
                        restoreMode: Binding.RestoreNone
                    }
                }

                Text {
                    objectName: "modulePeriodMismatchHint"
                    visible: moduleBridge.uniformPeriodIndex < 0
                    text: qsTr("各数值周期不一致")
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                }

                AppButton {
                    objectName: "modulePeriodApplyButton"
                    text: qsTr("应用到全部数值")
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    text: qsTr("当前基准周期 %1ms").arg(moduleBridge.basePeriodMs)
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Metrics.cardSpacing

            // 网格视图
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !page.detailVisible
                spacing: Metrics.cardSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingMd

                    AppTextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("搜索插件名 / 作者 / 标签")
                        onTextChanged: page.searchText = text
                    }

                    AppComboBox {
                        Layout.preferredWidth: Metrics.navWidth
                        model: page.categoryOptions
                        currentIndex: page.categoryIndex
                        onActivated: function (index) {
                            page.categoryIndex = index;
                        }
                    }

                    AppSegment {
                        model: [qsTr("网格"), qsTr("列表")]
                        currentIndex: page.viewMode
                        onSelected: function (index) {
                            page.viewMode = index;
                        }
                    }
                }

                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: page.visiblePlugins().length > 0
                    contentWidth: width
                    contentHeight: cardGrid.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    GridLayout {
                        id: cardGrid

                        width: parent.width
                        columns: page.viewMode === 0 ? 2 : 1
                        columnSpacing: Metrics.cardSpacing
                        rowSpacing: Metrics.cardSpacing

                        Repeater {
                            model: page.visiblePlugins()

                            delegate: PluginCard {
                                required property var modelData

                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                item: modelData
                                selected: String(page.selectedFile) === String(modelData.fileName)
                                onOpenDetail: page.openDetail(modelData.fileName)
                                onToggleEnabled: moduleBridge.togglePlugin(String(modelData.fileName))
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: page.visiblePlugins().length === 0
                    text: qsTr("没有匹配的插件")
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontSubtitle
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            // 详情视图
            PluginDetail {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: page.detailVisible && page.selectedPlugin !== null
                item: page.selectedPlugin
                eventsItem: page.selectedEvents
                onBack: page.detailVisible = false
            }

            // 已启用栏
            Rectangle {
                Layout.preferredWidth: page.sidebarWidth
                Layout.fillHeight: true
                visible: !page.detailVisible
                radius: Metrics.cardRadius
                color: Theme.surface
                border.width: Metrics.borderWidth
                border.color: Theme.border

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Metrics.cardPadding
                    spacing: Metrics.spacingMd

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingSm

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("已启用")
                            color: Theme.textPrimary
                            font.pixelSize: Typography.fontSubtitle
                            font.bold: true
                        }

                        Text {
                            text: page.enabledPlugins().length.toString()
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("可同时运行多个插件。各通道取最高强度，不叠加。")
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontBody
                        wrapMode: Text.WordWrap
                    }

                    Flickable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: width
                        contentHeight: enabledColumn.implicitHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        Column {
                            id: enabledColumn

                            width: parent.width
                            spacing: Metrics.spacingSm

                            Repeater {
                                model: page.enabledPlugins()

                                delegate: Rectangle {
                                    required property var modelData

                                    width: enabledColumn.width
                                    implicitHeight: entryColumn.implicitHeight + Metrics.spacingMd * 2
                                    radius: Metrics.radiusSm
                                    color: Theme.surfaceAlt
                                    border.width: Metrics.borderWidth
                                    border.color: Theme.border

                                    ColumnLayout {
                                        id: entryColumn

                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: Metrics.spacingMd
                                        spacing: Metrics.spacingXs

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Metrics.spacingSm

                                            Text {
                                                Layout.fillWidth: true
                                                text: String(modelData.name)
                                                color: Theme.textPrimary
                                                font.pixelSize: Typography.fontBody
                                                elide: Text.ElideRight
                                            }

                                            Text {
                                                text: qsTr("运行中")
                                                color: Theme.success
                                                font.pixelSize: Typography.fontBody
                                            }
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Metrics.spacingSm

                                            Text {
                                                id: configLink

                                                text: qsTr("配置 →")
                                                color: Theme.accent
                                                font.pixelSize: Typography.fontBody

                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: page.openDetail(modelData.fileName)
                                                }
                                            }

                                            Item {
                                                Layout.fillWidth: true
                                            }

                                            AppButton {
                                                text: qsTr("停用")
                                                onClicked: moduleBridge.togglePlugin(String(modelData.fileName))
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    AppButton {
                        Layout.fillWidth: true
                        text: qsTr("查看运行状态 →")
                        onClicked: app.navigate(0)
                    }
                }
            }
        }
    }
}
