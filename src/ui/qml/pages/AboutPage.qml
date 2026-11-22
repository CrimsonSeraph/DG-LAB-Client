import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Dglab

// 关于页：README 风格的软件介绍 + 许可证与依赖。
// 头部（图标 / 标题 / 徽标 / 副标题）用独立的 Image 与 Text 排布，正文才交给 Text.MarkdownText
// —— 后者的 Markdown 支持不渲染图片。交互约定：本文件不写信号处理器。
Item {
    id: page

    objectName: "aboutPage"

    // README 头部度量：图标边长与徽标高都从令牌派生，不写裸尺寸
    readonly property int iconSize: Metrics.spacing3xl * 3
    readonly property int badgeHeight: ComponentStyle.statusBadgeHeight
    // 三个 shields 徽标共用的主色：这是远程资源 URL 的 color 参数（与仓库 README 头部
    // version 徽标取同一个值），不是本地配色令牌，因此不来自 Theme。
    readonly property string badgeColor: "ff4d8d"

    // 正文的 Markdown 源：一个元素是一段，段间空行成段。列表没有逐条拆行，
    // 保持每段一个翻译单元（qsTr 的粒度是整段而不是句子）
    readonly property var introBlocks: [
        qsTr("## 这是什么"),
        qsTr("DG-LAB-Client 是一个跑在电脑上的 DG-LAB 控制端。设备连到电脑之后，强度、波形与自动化规则都在这一个窗口里管理，不必在手机和电脑之间来回切换。"),
        qsTr("## 怎么连上设备"),
        qsTr("最省事的做法是用手机 App 扫一下屏幕上的二维码：中转服务由应用自己带起来，电脑上不需要再装别的运行环境。也可以用蓝牙直接把郊狼 V3 连上；负鼠、爪印与灵猫的驱动还在验证，扫描列表会在每台设备后面写明支持程度，连不上的会给出原因。"),
        qsTr("## 用它做什么"),
        qsTr("两个通道分别调整强度与波形，强度受上限约束。波形库里有内置波形，也可以自己编辑一段发过去。"),
        qsTr("不想每次手动调的话，可以把条件写成规则，让输出跟着情势走；插件上报的事件也能映射成强度、百分比或一段波形。插件负责从游戏、传感器、无线按钮这类来源读数据，在插件中心启停与配置，插件市场里按需安装或更新。"),
        qsTr("迷你窗置顶显示两路强度与滚动波形，调参时不用切回主窗口；关掉主窗口会收进托盘继续运行。"),
        qsTr("## 外观"),
        qsTr("内置多套预设主题，也可以自定义主色与副色，整个界面一起更换。")
    ]

    ScrollView {
        id: scrollView

        objectName: "aboutScrollView"
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
                title: qsTr("关于")
                subtitle: "v" + app.appVersion
            }

            GlassCard {
                Layout.fillWidth: true
                title: qsTr("软件信息")

                // 头部与项目 README 同构：图标 → 标题 → 三个徽标 → 一句副标题，整块居中
                Column {
                    objectName: "aboutBrandBlock"

                    width: parent.width
                    spacing: Metrics.spacingMd

                    // 随包分发的本地图标（QRC），离线也有图
                    Image {
                        objectName: "aboutAppIcon"

                        x: (parent.width - width) / 2
                        source: "qrc:/icon/assets/icon/dglab-client-128.png"
                        sourceSize.width: page.iconSize
                        sourceSize.height: page.iconSize
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }

                    Text {
                        objectName: "aboutAppTitle"

                        width: parent.width
                        text: app.appName
                        color: Theme.textPrimary
                        font.pixelSize: Typography.fontTitle
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }

                    // 三个 shields 徽标：数据源与仓库 README 头部一致（version / license / stars），
                    // 并统一到同一套远程参数——同一个 color、显式同一个 style，都不挂 logo，
                    // 因此并排时配色与字形一致（默认色随接口变化：license 是橙、stars 是灰）。
                    // style 显式写 flat：stars 接口在缺省 style 时会忽略 color，只有带上 style 才上色。
                    // 远程动态图：离线或加载失败时就地隐藏，不占位也不留空框（Row 的宽度随可见项收缩）
                    Row {
                        objectName: "aboutBadgeRow"

                        x: (parent.width - width) / 2
                        spacing: Metrics.spacingSm

                        Image {
                            objectName: "aboutVersionBadge"

                            visible: status === Image.Ready
                            source: "https://img.shields.io/badge/dynamic/json?url=https%3A%2F%2Fraw.githubusercontent.com%2FCrimsonSeraph%2FDG-LAB-Client%2Fmain%2Fconfig%2Fmain.json&query=%24.app.version&label=version&color="
                                    + page.badgeColor + "&style=flat"
                            sourceSize.height: page.badgeHeight
                            asynchronous: true
                        }

                        Image {
                            objectName: "aboutLicenseBadge"

                            visible: status === Image.Ready
                            source: "https://img.shields.io/github/license/CrimsonSeraph/DG-LAB-Client?color=" + page.badgeColor
                                    + "&style=flat"
                            sourceSize.height: page.badgeHeight
                            asynchronous: true
                        }

                        Image {
                            objectName: "aboutStarsBadge"

                            visible: status === Image.Ready
                            // 动态 stars 徽标的 SVG 含 rgba 透明矩形，Qt 的 SVG 渲染器会把它画成不透明白色
                            // 因此改用静态徽标，数值在发版时与根 README 一并更新
                            source: "https://img.shields.io/badge/stars-8-" + page.badgeColor + "?style=flat"
                            sourceSize.height: page.badgeHeight
                            asynchronous: true
                        }
                    }

                    Text {
                        objectName: "aboutSubtitleText"

                        width: parent.width
                        text: qsTr("一个基于 Qt/QML 的 DG-LAB 桌面客户端 · 自建中转 · 郊狼 V3 蓝牙直连")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.fontBody
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }

                // 正文：Markdown 渲染，标题 / 段落 / 强调都按 Markdown 语法解析
                Text {
                    objectName: "aboutIntroText"

                    width: parent.width
                    textFormat: Text.MarkdownText
                    text: page.introBlocks.join("\n\n")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                    linkColor: Theme.accent
                    wrapMode: Text.WordWrap
                }
            }

            GlassCard {
                Layout.fillWidth: true
                title: qsTr("许可证与依赖")

                Text {
                    objectName: "aboutLicenseText"
                    width: parent.width
                    text: qsTr("本软件以 GPL-3.0-only 许可证发布：可以自由使用、修改和分发，修改后的版本需以同一许可证开源。")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                    wrapMode: Text.WordWrap
                }

                Text {
                    objectName: "aboutDependenciesText"
                    width: parent.width
                    text: qsTr("用到的第三方组件主要是 Qt 6（界面、网络、蓝牙与并发），各自遵循原有许可证。")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                    wrapMode: Text.WordWrap
                }

                Text {
                    objectName: "aboutRepositoryText"
                    width: parent.width
                    text: qsTr("项目地址：github.com/CrimsonSeraph/DG-LAB-Client")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontBody
                    wrapMode: Text.WordWrap
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
