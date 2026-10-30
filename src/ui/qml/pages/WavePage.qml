import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 波形设置页：目标通道、A/B 波形预览、波形库入口、按分类展开的标签与可折叠的波形列表。
// 内容随窗口高度滚动（外层 ScrollView 撑满窗口），页内分组只用「小标题 + 留白」。
// 交互约定：本文件只有两处指针手势处理器（分类标题双击改名、拖动手柄调序，
// 登记在 src/ui/README.md「信号处理器例外」），其余交互控件由 C++（UiConnector）按 objectName 连接。
Item {
    id: page

    objectName: "wavePage"

    // 展开集合与拖动 / 改名状态都是纯视图状态：只在本文件里维护，
    // 点击展开由 C++ 按 objectName 翻转集合中的一项，拖动与改名由本文件的函数改写。
    property var expandedCategories: []
    property var expandedWaves: []
    property string renamingCategory: ""
    property var categoryDrag: ({
            active: false,
            name: "",
            fromIndex: -1,
            targetIndex: -1,
            restore: []
        })

    // 内置分类固定连在最前：拖动调序的目标下标不越过这一段
    readonly property int builtinCategoryCount: {
        const categories = waveBridge.tagCategories;
        let count = 0;
        for (let i = 0; i < categories.length; ++i) {
            if (!categories[i].builtin) {
                break;
            }
            ++count;
        }
        return count;
    }

    // 目标通道当前是否绑定了波形：清除入口按它表达可用性
    readonly property bool targetWaveBound: {
        const current = waveBridge.targetChannel === "A" ? waveBridge.currentA : waveBridge.currentB;
        return current.length > 0;
    }

    // 波形的强度序列：每帧 4 个 25ms 子单元，strength 取 0~100
    function strengthSeries(name) {
        const points = waveBridge.wavePoints(name);
        const series = [];
        for (let i = 0; i < points.length; ++i) {
            series.push(points[i].strength);
        }
        return series;
    }

    // 点击某一行的标题：C++ 按 objectName 找到点击项后调本函数翻转集合中的一项
    function toggleExpanded(propertyName, name) {
        const fromWaves = propertyName === "expandedWaves";
        const next = (fromWaves ? page.expandedWaves : page.expandedCategories).slice();
        const at = next.indexOf(name);
        if (at >= 0) {
            next.splice(at, 1);
        } else {
            next.push(name);
        }
        if (fromWaves) {
            page.expandedWaves = next;
        } else {
            page.expandedCategories = next;
        }
    }

    // 改写分类展开集合（false 用于拖动期间强制收起）
    function setCategoryExpanded(name, expanded) {
        const next = page.expandedCategories.slice();
        const at = next.indexOf(name);
        if (expanded && at < 0) {
            next.push(name);
        } else if (!expanded && at >= 0) {
            next.splice(at, 1);
        } else {
            return;
        }
        page.expandedCategories = next;
    }

    // 双击标题开始就地改名：内置分类（含「用户自定义」）名称固定，不进入改名状态
    function startCategoryRename(name) {
        const categories = waveBridge.tagCategories;
        for (let i = 0; i < categories.length; ++i) {
            if (categories[i].name !== name) {
                continue;
            }
            if (!categories[i].builtin) {
                page.renamingCategory = name;
            }
            return;
        }
    }

    // 改名提交：与现有分类重名由桥接拒绝并经 operationFailed 提示，原名保持不变
    function commitCategoryRename(name, value) {
        if (page.renamingCategory !== name) {
            return;
        }
        page.renamingCategory = "";
        const trimmed = value.trim();
        if (trimmed.length === 0 || trimmed === name) {
            return;
        }
        waveBridge.renameTagCategory(name, trimmed);
    }

    // 拖动开始：被拖动的分类先收起，收起态行高一致才好把位移换算成目标下标
    function beginCategoryDrag(name, index) {
        page.categoryDrag = {
            active: true,
            name: name,
            fromIndex: index,
            targetIndex: index,
            restore: page.expandedCategories.indexOf(name) >= 0 ? [name] : []
        };
        page.setCategoryExpanded(name, false);
    }

    // 拖动中：按位移更新目标下标，途经的让位分类一并收起并记下原本展开的
    function updateCategoryDrag(deltaY) {
        const drag = page.categoryDrag;
        if (!drag.active) {
            return;
        }
        const categories = waveBridge.tagCategories;
        const step = ComponentStyle.waveTagHeaderHeight + Metrics.spacingXs;
        const moved = Math.round(deltaY / step);
        let target = drag.fromIndex + moved;
        target = Math.max(page.builtinCategoryCount, Math.min(categories.length - 1, target));
        if (target === drag.targetIndex) {
            return;
        }
        const restore = drag.restore.slice();
        const first = Math.min(drag.fromIndex, target);
        const last = Math.max(drag.fromIndex, target);
        for (let i = first; i <= last; ++i) {
            const name = categories[i].name;
            if (name === drag.name || restore.indexOf(name) >= 0) {
                continue;
            }
            if (page.expandedCategories.indexOf(name) >= 0) {
                restore.push(name);
            }
            page.setCategoryExpanded(name, false);
        }
        page.categoryDrag = {
            active: true,
            name: drag.name,
            fromIndex: drag.fromIndex,
            targetIndex: target,
            restore: restore
        };
    }

    // 拖动结束：先恢复让位分类原本的展开态，再提交调序（被拖动的分类保持收起）
    function endCategoryDrag() {
        const drag = page.categoryDrag;
        if (!drag.active) {
            return;
        }
        page.categoryDrag = {
            active: false,
            name: "",
            fromIndex: -1,
            targetIndex: -1,
            restore: []
        };
        for (let i = 0; i < drag.restore.length; ++i) {
            if (drag.restore[i] !== drag.name) {
                page.setCategoryExpanded(drag.restore[i], true);
            }
        }
        if (drag.targetIndex === drag.fromIndex || drag.targetIndex < page.builtinCategoryCount) {
            return;
        }
        // 调序会让分类模型整表重建并销毁当前委托：提交放到本次手势回调之后
        Qt.callLater(function () {
            waveBridge.moveTagCategory(drag.name, drag.targetIndex);
        });
    }

    ScrollView {
        id: scrollView

        objectName: "waveScrollView"
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        padding: Metrics.pageMargin
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: Metrics.cardSpacing

            PageHeader {
                Layout.fillWidth: true
                title: qsTr("波形设置")
                subtitle: qsTr("波形库管理与 A/B 通道波形指派 · 当前目标通道 %1").arg(waveBridge.targetChannel)
            }

            GlassCard {
                objectName: "waveChannelCard"
                Layout.fillWidth: true
                title: qsTr("通道波形")

                Column {
                    width: parent.width
                    spacing: ComponentStyle.groupSpacing

                    RowLayout {
                        width: parent.width
                        spacing: Metrics.spacingMd

                        Text {
                            text: qsTr("目标通道")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        // 选中态沿用 primary 语义：当前编辑目标通道整块强调，另一个保持默认表面色
                        AppButton {
                            objectName: "waveChannelAButton"
                            text: "A"
                            primary: waveBridge.targetChannel === "A"
                            Layout.preferredWidth: Metrics.buttonHeight * 2
                            Layout.minimumWidth: Layout.preferredWidth
                            Layout.maximumWidth: Layout.preferredWidth
                        }

                        AppButton {
                            objectName: "waveChannelBButton"
                            text: "B"
                            primary: waveBridge.targetChannel === "B"
                            Layout.preferredWidth: Metrics.buttonHeight * 2
                            Layout.minimumWidth: Layout.preferredWidth
                            Layout.maximumWidth: Layout.preferredWidth
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Text {
                            text: qsTr("当前：A = %1 ｜ B = %2").arg(waveBridge.currentA.length > 0 ? waveBridge.currentA : qsTr("未选择")).arg(waveBridge.currentB.length > 0 ? waveBridge.currentB : qsTr("未选择"))
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }
                    }

                    // A 与 B 平级：两列权重相同（preferredWidth 0 + fillWidth），
                    // 文本宽度与预览框隐含宽度都不参与列宽分配，只由布局权重等分。
                    RowLayout {
                        width: parent.width
                        spacing: Metrics.spacingLg

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            spacing: Metrics.spacingXs

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("通道 A")
                                color: Theme.textSecondary
                                font.pixelSize: Typography.fontBody
                                elide: Text.ElideRight
                            }

                            WavePreview {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 0
                                Layout.preferredHeight: ComponentStyle.wavePreviewHeight
                                points: waveBridge.pointsA
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            spacing: Metrics.spacingXs

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("通道 B")
                                color: Theme.textSecondary
                                font.pixelSize: Typography.fontBody
                                elide: Text.ElideRight
                            }

                            WavePreview {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 0
                                Layout.preferredHeight: ComponentStyle.wavePreviewHeight
                                points: waveBridge.pointsB
                            }
                        }
                    }

                    // 卡片底部行：导入 / 导出在左，选择 / 清除 / 创建波形在右
                    RowLayout {
                        width: parent.width
                        spacing: Metrics.spacingMd

                        AppButton {
                            objectName: "waveImportButton"
                            text: qsTr("导入")
                        }

                        AppButton {
                            objectName: "waveExportButton"
                            text: qsTr("导出")
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        AppButton {
                            objectName: "waveSelectButton"
                            text: qsTr("选择波形")
                        }

                        AppButton {
                            objectName: "waveClearButton"
                            text: qsTr("清除波形")
                            enabled: page.targetWaveBound
                        }

                        AppButton {
                            objectName: "waveCreateButton"
                            text: qsTr("创建波形")
                        }
                    }
                }
            }

            // 波形标签：分类展开栏（顺序即 waveBridge.tagCategories 的顺序）+ 可折叠的波形列表
            GlassCard {
                objectName: "waveTagCard"
                Layout.fillWidth: true
                title: qsTr("波形标签")

                Column {
                    width: parent.width
                    spacing: ComponentStyle.groupSpacing

                    RowLayout {
                        width: parent.width
                        spacing: Metrics.spacingMd

                        Text {
                            text: qsTr("标签分类")
                            color: Theme.textSecondary
                            font.pixelSize: Typography.fontBody
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        AppTextField {
                            objectName: "waveTagNewCategoryField"
                            Layout.preferredWidth: ComponentStyle.inputMinWidth * 2
                            placeholderText: qsTr("新建分类名")
                        }

                        AppButton {
                            objectName: "waveTagNewCategoryButton"
                            text: qsTr("新建分类")
                            primary: true
                        }
                    }

                    // 分类展开栏：默认全部收起，点标题行展开该分类的标签
                    Column {
                        id: waveTagCategoryList

                        objectName: "waveTagCategoryList"
                        width: parent.width
                        spacing: Metrics.spacingXs

                        Repeater {
                            model: waveBridge.tagCategories

                            delegate: Column {
                                id: categoryBlock

                                readonly property string categoryName: modelData.name
                                readonly property bool categoryBuiltin: modelData.builtin
                                readonly property int categoryIndex: index
                                readonly property bool categoryExpanded: page.expandedCategories.indexOf(categoryName) >= 0
                                readonly property bool categoryRenaming: page.renamingCategory === categoryName

                                width: waveTagCategoryList.width
                                spacing: 0

                                // 标题行：内置分类只显示名称；自建分类可双击改名，右侧手柄按住上下拖动调序
                                Rectangle {
                                    id: categoryHeader

                                    width: parent.width
                                    height: ComponentStyle.waveTagHeaderHeight
                                    radius: Metrics.radiusSm
                                    color: Theme.surfaceAlt
                                    border.width: Metrics.borderWidth
                                    border.color: Theme.divider
                                    z: reorderDrag.active ? 1 : 0
                                    transform: Translate {
                                        y: reorderDrag.active ? reorderDrag.activeTranslation.y : 0
                                    }

                                    // 整行热区：点击展开 / 收起由 C++ 连接，双击自建分类就地改名
                                    MouseArea {
                                        objectName: "waveTagCategoryHeader"
                                        anchors.fill: parent
                                        enabled: !categoryBlock.categoryRenaming
                                        property string categoryName: categoryBlock.categoryName
                                        onDoubleClicked: page.startCategoryRename(categoryName)
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: Metrics.spacingMd
                                        anchors.rightMargin: Metrics.spacingXs
                                        spacing: Metrics.spacingSm

                                        AppIcon {
                                            icon: categoryBlock.categoryExpanded ? "f078" : "f054"
                                            size: Typography.fontBody
                                            color: Theme.textMuted
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            visible: !categoryBlock.categoryRenaming
                                            text: categoryBlock.categoryName
                                            color: Theme.textPrimary
                                            font.pixelSize: Typography.fontBody
                                            elide: Text.ElideRight
                                        }

                                        AppTextField {
                                            objectName: "waveTagCategoryRenameField"
                                            Layout.fillWidth: true
                                            visible: categoryBlock.categoryRenaming
                                            focus: categoryBlock.categoryRenaming
                                            text: categoryBlock.categoryName
                                            onEditingFinished: page.commitCategoryRename(categoryBlock.categoryName, text)
                                        }

                                        // 调序手柄：三条横线，按住上下拖动，内置分类不提供
                                        AppIcon {
                                            objectName: "waveTagCategoryHandle"
                                            visible: !categoryBlock.categoryBuiltin
                                            icon: "f0c9"
                                            size: Typography.fontBody
                                            color: Theme.textMuted
                                            Layout.preferredWidth: ComponentStyle.waveTagHandleWidth
                                            Layout.preferredHeight: ComponentStyle.waveTagHeaderHeight

                                            HoverHandler {
                                                cursorShape: Qt.SizeVerCursor
                                            }

                                            DragHandler {
                                                id: reorderDrag

                                                target: null
                                                xAxis.enabled: false
                                                yAxis.enabled: true
                                                onActiveChanged: {
                                                    if (active) {
                                                        page.beginCategoryDrag(categoryBlock.categoryName, categoryBlock.categoryIndex);
                                                    } else {
                                                        page.endCategoryDrag();
                                                    }
                                                }
                                                onActiveTranslationChanged: page.updateCategoryDrag(activeTranslation.y)
                                            }
                                        }
                                    }
                                }

                                // 展开内容：该分类的标签列表 + 新建标签 / 删除分类入口
                                Column {
                                    width: parent.width
                                    visible: categoryBlock.categoryExpanded
                                    topPadding: Metrics.spacingXs
                                    spacing: Metrics.spacingXs

                                    Repeater {
                                        model: modelData.tags

                                        delegate: Rectangle {
                                            width: categoryBlock.width
                                            height: Metrics.buttonHeight + Metrics.spacingMd
                                            radius: Metrics.radiusSm
                                            color: Theme.surface
                                            border.width: Metrics.borderWidth
                                            border.color: Theme.divider

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.margins: Metrics.spacingXs
                                                anchors.leftMargin: Metrics.spacingMd
                                                spacing: Metrics.spacingMd

                                                AppIcon {
                                                    visible: modelData.builtin
                                                    icon: "f023"
                                                    size: Typography.fontBody
                                                    color: Theme.textMuted
                                                }

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: modelData.name
                                                    color: Theme.textPrimary
                                                    font.pixelSize: Typography.fontBody
                                                    elide: Text.ElideRight
                                                }

                                                Text {
                                                    visible: modelData.usage > 0
                                                    text: qsTr("%1 个波形在用").arg(modelData.usage)
                                                    color: Theme.textMuted
                                                    font.pixelSize: Typography.fontBody
                                                }

                                                // 内置标签不提供删除入口
                                                AppButton {
                                                    objectName: "waveTagCategoryTagDelete"
                                                    visible: !modelData.builtin
                                                    property string tagName: modelData.name
                                                    text: qsTr("删除")
                                                    danger: true
                                                    outline: true
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        width: parent.width
                                        visible: modelData.tags.length === 0
                                        text: qsTr("该分类还没有标签")
                                        color: Theme.textMuted
                                        font.pixelSize: Typography.fontBody
                                    }

                                    RowLayout {
                                        width: parent.width
                                        spacing: Metrics.spacingMd

                                        AppTextField {
                                            id: categoryNewTagField
                                            objectName: "waveTagCategoryNewField"
                                            Layout.fillWidth: true
                                            placeholderText: qsTr("在该分类下新建标签")
                                        }

                                        // 新标签名经属性绑定交给 C++，委托里的重复控件不为每个实例写处理器
                                        AppButton {
                                            objectName: "waveTagCategoryCreateClick"
                                            property string categoryName: categoryBlock.categoryName
                                            property string tagName: categoryNewTagField.text
                                            text: qsTr("新建标签")
                                        }

                                        // 删除自建分类：二次确认由 C++ 走既有的 confirmDialog
                                        AppButton {
                                            objectName: "waveTagCategoryRemoveClick"
                                            visible: !categoryBlock.categoryBuiltin
                                            property string categoryName: categoryBlock.categoryName
                                            text: qsTr("删除分类")
                                            danger: true
                                            outline: true
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        text: qsTr("波形列表 · 点击行展开静态波形图")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                    }

                    Column {
                        id: waveTagWaveList

                        objectName: "waveTagWaveList"
                        width: parent.width
                        spacing: Metrics.spacingXs

                        Repeater {
                            model: waveBridge.waves

                            delegate: Rectangle {
                                width: waveTagWaveList.width
                                height: waveEntry.implicitHeight + Metrics.spacingMd * 2
                                radius: Metrics.radiusSm
                                color: Theme.surfaceAlt
                                border.width: Metrics.borderWidth
                                border.color: Theme.divider

                                // 标题行热区：只覆盖标题行，行内按钮与标签条在它上层，各自接收事件
                                MouseArea {
                                    objectName: "waveTagWaveHeader"
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    height: waveEntryRow.height + Metrics.spacingMd
                                    property string waveName: modelData.name
                                }

                                // 内容贴顶排列：展开静态波形图时标题行位置不动，整行高度随内容撑开
                                Column {
                                    id: waveEntry
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: Metrics.spacingMd
                                    spacing: Metrics.spacingXs

                                    // 名称与标签同一行：名称占满剩余宽度并省略号截断，
                                    // 标签条最多占行宽的 40%，超出部分裁剪后可用滚轮 / 拖动横向查看
                                    RowLayout {
                                        id: waveEntryRow

                                        width: parent.width
                                        spacing: Metrics.spacingMd

                                        Text {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            text: modelData.name
                                            color: Theme.textPrimary
                                            font.pixelSize: Typography.fontBody
                                            elide: Text.ElideRight
                                        }

                                        AppTagStrip {
                                            tags: modelData.tags
                                            Layout.alignment: Qt.AlignVCenter
                                            Layout.maximumWidth: Math.round(waveEntryRow.width * 0.4)
                                        }

                                        Text {
                                            text: modelData.official ? qsTr("内置只读") : qsTr("%1 帧 · %2ms").arg(modelData.frameCount).arg(modelData.durationMs)
                                            color: Theme.textMuted
                                            font.pixelSize: Typography.fontBody
                                        }

                                        // 标签可编辑的对象含官方波形：官方条目的帧与名称只读，标签是用户侧叠加数据
                                        AppButton {
                                            objectName: "waveTagEditClick"
                                            property string waveName: modelData.name
                                            text: qsTr("编辑标签")
                                        }
                                    }

                                    // 展开内容：该波形的静态波形图（strength 序列，取值 0~100）
                                    AppSparkline {
                                        objectName: "waveTagWavePreview"
                                        width: parent.width
                                        height: ComponentStyle.waveTagWaveChartHeight
                                        visible: page.expandedWaves.indexOf(modelData.name) >= 0
                                        maxValue: 100
                                        // wavePoints 是 Q_INVOKABLE：收起态不必逐行解析帧，展开时才取序列
                                        points: visible ? page.strengthSeries(modelData.name) : []
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item {
                Layout.preferredHeight: Metrics.pageMargin
            }
        }
    }
}
