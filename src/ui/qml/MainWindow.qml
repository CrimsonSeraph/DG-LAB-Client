import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 主窗口：左侧导航 + 中央页面栈 + 底部状态栏。
//
// 交互约定（项目硬性规范）：
//  - 本文件没有任何 onClicked / Connections / onXxx 处理器；
//  - 每个交互控件都有稳定的 objectName，由 C++（UiConnector）在 engine.load 后
//    显式建立连接；界面只通过属性绑定读取 app / Theme 的状态。
//
// 样式约定：颜色取 Theme（C++ 单例），间距/圆角取 Metrics，字号取 Typography，
//          断点取 Responsive，组件度量取 ComponentStyle，页面内不写颜色字面量。
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
        width: root.compactNav ? 64 : Metrics.navWidth
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
                    visible: !root.compactNav
                    Layout.fillWidth: true
                    spacing: 0

                    Text {
                        text: app.appName
                        color: Theme.textPrimary
                        font.pixelSize: Typography.fontBodyLarge
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "v" + app.appVersion
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontCaption
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }

            NavButton {
                objectName: "navHomeButton"
                text: root.compactNav ? qsTr("首") : qsTr("首页")
                selected: app.currentPage === 0
                Layout.fillWidth: true
            }

            NavButton {
                objectName: "navConfigButton"
                text: root.compactNav ? qsTr("配") : qsTr("配置")
                selected: app.currentPage === 1
                Layout.fillWidth: true
            }

            NavButton {
                objectName: "navModuleButton"
                text: root.compactNav ? qsTr("模") : qsTr("模块")
                selected: app.currentPage === 2
                Layout.fillWidth: true
            }

            NavButton {
                objectName: "navAboutButton"
                text: root.compactNav ? qsTr("关") : qsTr("关于")
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

    // 波形库与波形编辑器（首页 / 配置页均可打开）
    WaveSelectDialog {
        id: waveSelectDialog
    }

    WaveEditorDialog {
        id: waveEditorDialog
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
            text: app.statusText.length > 0 ? app.statusText : qsTr("就绪")
            color: Theme.textSecondary
            font.pixelSize: Typography.fontSmall
            elide: Text.ElideRight
        }
    }
}
