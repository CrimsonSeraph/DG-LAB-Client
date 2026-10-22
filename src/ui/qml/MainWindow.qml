import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 主窗口：左侧导航 + 中央页面栈 + 底部状态栏。
// 本文件不写任何信号处理器，交互与样式约定见 src/ui/README.md。
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

    // 左侧导航
    Rectangle {
        id: navigationBar

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.compactNav ? Metrics.navWidthCompact : Metrics.navWidth
        color: Theme.surfaceAlt

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Metrics.spacingMd
            spacing: Metrics.spacingXs

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Metrics.navHeaderHeight
                spacing: Metrics.spacingSm

                Image {
                    source: "qrc:/image/assets/normal_image/main_image.png"
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

            NavButton {
                objectName: "navHomeButton"
                text: qsTr("首页")
                selected: app.currentPage === 0
                Layout.fillWidth: true
            }

            NavButton {
                objectName: "navConfigButton"
                text: qsTr("配置")
                selected: app.currentPage === 1
                Layout.fillWidth: true
            }

            NavButton {
                objectName: "navModuleButton"
                text: qsTr("模块")
                selected: app.currentPage === 2
                Layout.fillWidth: true
            }

            NavButton {
                objectName: "navAboutButton"
                text: qsTr("关于")
                selected: app.currentPage === 3
                Layout.fillWidth: true
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }

    // 中央页面栈：C++ 侧通过 app.currentPage 完成导航
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
            text: qsTr("中转：%1 · 蓝牙：%2 · 通道：%3")
                .arg(device.connected ? qsTr("已连接") : qsTr("未连接"))
                .arg(ble.connected ? qsTr("已连接") : qsTr("未连接"))
                .arg(home.channelAState === "idle" && home.channelBState === "idle" ? qsTr("空闲") : qsTr("运行中"))
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
            elide: Text.ElideRight
        }
    }
}
