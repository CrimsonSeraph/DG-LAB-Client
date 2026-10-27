import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 主窗口：左侧导航（图标 + 文字）+ 内容区 + 底部状态栏。
// 交互约定：本文件不写信号处理器，导航与左下角面板入口由 C++（UiConnector）按 objectName 连接。
ApplicationWindow {
    id: root

    objectName: "mainWindow"
    width: 1280
    height: 800
    minimumWidth: Responsive.minWindowWidth
    minimumHeight: Responsive.minWindowHeight
    visible: true
    title: app.appName + " [" + app.appVersion + "]"
    color: Theme.windowBg

    readonly property bool compactNav: Responsive.isCompactNav(width)

    // 页面索引与 AppBridge::Page 的枚举值逐项对应，同时决定 pageStack 的条目顺序
    readonly property int pageHome: 0
    readonly property int pagePluginCenter: 4
    readonly property int pageWave: 5
    readonly property int pageMiniGame: 6
    readonly property int pageLog: 7

    // 连接状态统一口径：蓝牙连接（0/1）+ APP 在线客户端数（来源 device.appClientCount）
    readonly property int bleCount: ble.connected ? 1 : 0
    readonly property int appCount: device.appClientCount
    readonly property int connectionCount: bleCount + appCount

    // 导航项内容：图标 + 文字。NavButton 默认只渲染文字，这里统一覆盖 contentItem。
    component NavItemContent: RowLayout {
        id: navContent

        property string glyph: ""
        property string label: ""
        property bool highlighted: false

        spacing: Metrics.spacingSm

        AppIcon {
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: Metrics.spacingLg
            icon: navContent.glyph
            size: Typography.fontHeading
            color: navContent.highlighted ? Theme.selectionText : Theme.textSecondary
        }

        Text {
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            text: navContent.label
            color: navContent.highlighted ? Theme.selectionText : Theme.textSecondary
            font.pixelSize: Typography.fontSubtitle
            font.bold: navContent.highlighted
            elide: Text.ElideRight
        }
    }

    // 左侧导航：页头钉顶、面板钉底，导航项夹在两者之间（空间不足时滚动）
    Rectangle {
        id: navigationBar

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.compactNav ? Metrics.navWidthCompact : Metrics.navWidth
        color: Theme.surfaceAlt

        // 应用名与版本
        RowLayout {
            id: navHeader

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Metrics.spacingMd
            height: Metrics.navHeaderHeight
            spacing: Metrics.spacingSm

            Image {
                source: "qrc:/icon/assets/icon/dglab-client-128.png"
                sourceSize.width: 40
                sourceSize.height: 40
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                fillMode: Image.PreserveAspectFit
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                spacing: 0

                Text {
                    text: app.appName
                    color: Theme.textPrimary
                    font.pixelSize: Typography.fontSubtitle
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    text: "v" + app.appVersion
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontBody
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        // 导航项：图标暂用几何字符前缀，待图标资源落地后替换为图标列
        Flickable {
            id: navScroll

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: navHeader.bottom
            anchors.bottom: panelFooter.top
            anchors.margins: Metrics.spacingMd
            contentWidth: width
            contentHeight: navColumn.implicitHeight
            interactive: contentHeight > height
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            Column {
                id: navColumn

                width: navScroll.width
                spacing: Metrics.spacingXs

                NavButton {
                    id: navHomeButton

                    objectName: "navHomeButton"
                    text: qsTr("主页")
                    selected: app.currentPage === root.pageHome
                    width: parent.width
                    contentItem: NavItemContent {
                        glyph: "f015" // house
                        label: navHomeButton.text
                        highlighted: navHomeButton.selected
                    }
                }

                NavButton {
                    id: navPluginCenterButton

                    objectName: "navPluginCenterButton"
                    text: qsTr("插件中心")
                    selected: app.currentPage === root.pagePluginCenter
                    width: parent.width
                    contentItem: NavItemContent {
                        glyph: "f12e" // puzzle-piece
                        label: navPluginCenterButton.text
                        highlighted: navPluginCenterButton.selected
                    }
                }

                NavButton {
                    id: navWaveButton

                    objectName: "navWaveButton"
                    text: qsTr("波形设置")
                    selected: app.currentPage === root.pageWave
                    width: parent.width
                    contentItem: NavItemContent {
                        glyph: "f83e" // wave-square
                        label: navWaveButton.text
                        highlighted: navWaveButton.selected
                    }
                }

                NavButton {
                    id: navMiniGameButton

                    objectName: "navMiniGameButton"
                    text: qsTr("小游戏")
                    selected: app.currentPage === root.pageMiniGame
                    width: parent.width
                    contentItem: NavItemContent {
                        glyph: "f11b" // gamepad
                        label: navMiniGameButton.text
                        highlighted: navMiniGameButton.selected
                    }
                }

                NavButton {
                    id: navLogButton

                    objectName: "navLogButton"
                    text: qsTr("日志")
                    selected: app.currentPage === root.pageLog
                    width: parent.width
                    contentItem: NavItemContent {
                        glyph: "f15c" // file-lines
                        label: navLogButton.text
                        highlighted: navLogButton.selected
                    }
                }
            }
        }

        // 左下角常驻面板：设备连接状态 + 设置 / 关于 / 检查更新
        Rectangle {
            id: panelFooter

            objectName: "panelFooter"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Metrics.spacingMd
            height: implicitHeight
            implicitHeight: footerColumn.implicitHeight + Metrics.spacingLg * 2
            radius: Metrics.radiusMd
            color: Theme.surface
            border.width: Metrics.borderWidth
            border.color: Theme.divider

            ColumnLayout {
                id: footerColumn

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Metrics.spacingLg
                spacing: Metrics.spacingSm

                // 设备连接状态（只读展示）：蓝牙与 APP 两种连接口径并列
                RowLayout {
                    objectName: "deviceStatusRow"

                    Layout.fillWidth: true
                    spacing: Metrics.spacingSm

                    Rectangle {
                        objectName: "deviceStatusDot"

                        Layout.alignment: Qt.AlignTop
                        implicitWidth: Metrics.spacingMd
                        implicitHeight: Metrics.spacingMd
                        radius: width / 2
                        color: root.connectionCount > 0 ? Theme.success : Theme.textMuted
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 0
                        spacing: 0

                        Text {
                            objectName: "deviceStatusName"

                            Layout.fillWidth: true
                            text: root.connectionCount > 0
                                ? qsTr("已连接设备 ×%1").arg(root.connectionCount)
                                : qsTr("未连接设备")
                            color: Theme.textPrimary
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }

                        Text {
                            objectName: "deviceStatusBle"

                            Layout.fillWidth: true
                            text: ble.connected ? qsTr("蓝牙已连接") : qsTr("蓝牙未连接")
                            color: ble.connected ? Theme.success : Theme.textMuted
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }

                        Text {
                            objectName: "deviceStatusApp"

                            Layout.fillWidth: true
                            text: root.appCount > 0 ? qsTr("App 已连接 ×%1").arg(root.appCount) : qsTr("App 未连接")
                            color: root.appCount > 0 ? Theme.success : Theme.textMuted
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }

                        Text {
                            objectName: "deviceStatusDevice"

                            Layout.fillWidth: true
                            visible: ble.connected && ble.deviceName !== ""
                            text: ble.deviceName
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                        }
                    }
                }

                AppButton {
                    objectName: "panelSettingsButton"
                    text: qsTr("设置")
                    Layout.fillWidth: true
                }

                AppButton {
                    objectName: "panelAboutButton"
                    text: qsTr("关于")
                    Layout.fillWidth: true
                }

                AppButton {
                    objectName: "panelCheckUpdateButton"
                    text: updateBridge.checking ? qsTr("检查中...") : qsTr("检查更新")
                    enabled: !updateBridge.checking && !updateBridge.downloading
                    Layout.fillWidth: true
                }
            }
        }
    }

    // 内容区页面栈：条目顺序与 AppBridge::Page 一致，C++ 侧通过 app.currentPage 完成导航
    StackLayout {
        id: pageStack

        objectName: "pageStack"
        anchors.left: navigationBar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: statusBar.top
        currentIndex: app.currentPage

        HomePage {
            id: homePage
        }

        ConfigPage {
            id: configPage
        }

        ModulePage {
            id: modulePage
        }

        AboutPage {
            id: aboutPage
        }

        PluginCenterPage {
            id: pluginCenterPage
        }

        WavePage {
            id: wavePage
        }

        MiniGamePage {
            id: miniGamePage
        }

        LogPage {
            id: logPage
        }
    }

    // 一次性反馈浮层：C++ 侧 app.showToast() 驱动，3 秒后自动消失
    Toast {
        id: toast
        bottomInset: Metrics.spacingXl + Metrics.statusBarHeight
    }

    // 波形库与波形编辑器（首页 / 配置页均可打开）
    WaveSelectDialog {
        id: waveSelectDialog
    }

    WaveEditorDialog {
        id: waveEditorDialog
    }

    // 规则可视化编辑器（首页 / 配置页入口打开）
    RuleEditorDialog {
        id: ruleEditorDialog
    }

    // 通用确认弹层（C++ 侧 UiConnector::ask_confirm 驱动）
    ConfirmDialog {
        id: confirmDialog
    }

    // 底部状态栏
    Rectangle {
        id: statusBar

        objectName: "statusBar"
        anchors.left: navigationBar.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Metrics.statusBarHeight
        color: Theme.surface
        border.width: Metrics.borderWidth
        border.color: Theme.divider

        Text {
            id: statusLabel

            objectName: "statusLabel"
            anchors.left: parent.left
            anchors.leftMargin: Metrics.spacingLg
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("蓝牙：%1 · App：%2 · 通道：%3")
                .arg(ble.connected ? qsTr("已连接") : qsTr("未连接"))
                .arg(root.appCount > 0 ? qsTr("已连接 ×%1").arg(root.appCount) : qsTr("未连接"))
                .arg(home.channelAState === "idle" && home.channelBState === "idle" ? qsTr("空闲") : qsTr("运行中"))
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
            elide: Text.ElideRight
        }
    }
}
