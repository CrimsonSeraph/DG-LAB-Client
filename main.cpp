/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "AppBridge.h"
#include "AppConfig.h"
#include "BleDeviceManager.h"
#include "Console.h"
#include "CoyoteBleController.h"
#include "DebugLog.h"
#include "DeviceController.h"
#include "DeviceValueBridge.h"
#include "EventBridge.h"
#include "EventRuntime.h"
#include "HomeBridge.h"
#include "LogBridge.h"
#include "ModuleBannerImageProvider.h"
#include "ModuleBridge.h"
#include "ModuleManager.h"
#include "RuleGraphBridge.h"
#include "RuleManager.h"
#include "ThemeManager.h"
#include "TrayController.h"
#include "UiConnector.h"
#include "UpdateBridge.h"
#include "UpdateManager.h"
#include "WaveBridge.h"
#include "WaveLibrary.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

#include <iostream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("CrimsonSeraph"));
    QCoreApplication::setApplicationName(QStringLiteral("DG-LAB-Client"));
    // 常驻托盘：最后一个窗口关闭不退出进程，退出只走托盘菜单
    QGuiApplication::setQuitOnLastWindowClosed(false);

    bool is_updated_start = false;
    for (const QString& arg : QCoreApplication::arguments()) {
        if (arg == "--updated") {
            is_updated_start = true;
            break;
        }
    }
    if (is_updated_start) {
        // 延迟 3 秒后生成标记，证明程序成功启动并运行
        QTimer::singleShot(3000, []() {
            const QString path = UpdateManager::update_flag_path();
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile flag(path);
            if (flag.open(QIODevice::WriteOnly)) {
                flag.write("ok");
                flag.close();
            }
        });
    }

    // 窗口图标：以 256 为基准，再补 64/32/16，任务栏与 Alt-Tab 在不同 DPI 下都有合适尺寸
    QIcon app_icon;
    app_icon.addFile(QStringLiteral(":/icon/assets/icon/dglab-client-256.png"));
    app_icon.addFile(QStringLiteral(":/icon/assets/icon/dglab-client-64.png"));
    app_icon.addFile(QStringLiteral(":/icon/assets/icon/dglab-client-32.png"));
    app_icon.addFile(QStringLiteral(":/icon/assets/icon/dglab-client-16.png"));
    QGuiApplication::setWindowIcon(app_icon);

    // 统一使用 Basic 样式：界面外观完全由 QML 的样式令牌控制（不叠加平台样式）
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // 配置初始化：先取一次日志单例，它必须比配置单例先构造、因而比后者后析构——
    // 配置单例在静态析构期仍会 save_all() 写日志，日志对象那时必须还活着
    (void)DebugLog::instance();
    auto& config = AppConfig::instance();
    try {
        if (!config.initialize("./config")) {
            DebugLog::instance().set_log_level("main", LOG_DEBUG);
            LOG_MODULE("main", "main", LOG_WARN, "配置系统初始化失败，使用内存配置");
        }
    }
    catch (const std::exception& e) {
        LOG_MODULE("main", "main", LOG_ERROR, "初始化时发生异常: " << e.what());
    }
    std::string error_msg;
    if (config.check_priority_conflict(error_msg)) {
        LOG_MODULE("main", "main", LOG_WARN, "优先级冲突: " << error_msg);
    }

    // 启用控制台
    const bool enable_console = config.get_value<bool>("app.debug", false);
    if (enable_console) {
        Console& console = Console::get_instance();
        if (console.create()) {
            LOG_MODULE("main", "main", LOG_DEBUG, "控制台已启用");
            LOG_MODULE("main", "main", LOG_INFO, "配置初始化完成，debug模式=" << enable_console);
        }
        else {
            LOG_MODULE("main", "main", LOG_WARN, "控制台启用失败（非 Windows 平台不支持）");
        }
    }

    const int console_log_level = config.get_value<int>("app.log.console_level", 0);
    DebugLog::instance().set_log_sink_level("console", static_cast<LogLevel>(console_log_level));
    LOG_MODULE("main", "main", LOG_DEBUG, "控制台日志级别设置为: " << console_log_level);
    const bool is_only_type_info = config.get_value<bool>("app.log.only_type_info", false);
    DebugLog::instance().set_only_type_info(is_only_type_info);

    // 文件日志：启用后写入可执行文件目录下的 log/
    const bool file_log_enabled = config.get_value<bool>("app.log.file_enabled", false);
    if (file_log_enabled) {
        const int file_log_level = config.get_value<int>("app.log.file_level", 0);
        const int file_log_max_mb = config.get_value<int>("app.log.file_max_mb", 5);
        const std::string file_log_dir = QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("log")).toStdString();
        const bool file_log_ok = DebugLog::instance().enable_file_sink(
            file_log_dir, static_cast<LogLevel>(file_log_level), file_log_max_mb);
        if (file_log_ok) {
            LOG_MODULE("main", "main", LOG_DEBUG, "文件日志已启用: " << file_log_dir
                << " level=" << file_log_level << " max_mb=" << file_log_max_mb);
        }
        else {
            LOG_MODULE("main", "main", LOG_WARN, "文件日志启用失败，已降级为不写文件: " << file_log_dir);
        }
    }

    // 主题令牌（QML 单例 Theme）
    ThemeManager theme;
    theme.initialize();
    qmlRegisterSingletonInstance("Dglab", 1, 0, "Theme", &theme);

    // 数值模块与规则引擎初始化（后续界面统一经桥接对象访问）
    ModuleManager::instance().init();
    RuleManager::instance().init();
    const auto rule_files = RuleManager::instance().get_available_rule_files();
    if (!rule_files.empty()) {
        try {
            RuleManager::instance().load_rule_file(rule_files.front());
        }
        catch (const std::exception& e) {
            LOG_MODULE("main", "main", LOG_ERROR, "加载默认规则文件失败: " << e.what());
        }
    }

    // 系统托盘常驻（QML 上下文属性 tray）
    TrayController tray;
    tray.initialize();

    // 界面日志桥接（QML 上下文属性 logBridge）
    LogBridge log_bridge;
    log_bridge.initialize();

    // 规则可视化编辑器桥接（QML 上下文属性 ruleGraph）
    RuleGraphBridge rule_graph;
    rule_graph.initialize();

    // 应用状态桥接（QML 上下文属性 app）
    AppBridge bridge;
    bridge.initialize();

    // 设备控制与首页桥接（QML 上下文属性 device / home）
    DeviceController device;
    device.initialize();
    HomeBridge home(&device);
    home.initialize();
    ModuleBridge module_bridge;
    module_bridge.initialize();
    // 事件运行时与事件桥接（QML 上下文属性 eventBridge）
    EventRuntime event_runtime;
    event_runtime.initialize();
    EventBridge event_bridge(&event_runtime);
    event_bridge.initialize();
    // 事件输出接入设备下发：每帧 A/B 强度与规则命令合并后下发，事件波形按帧推进播放与恢复
    QObject::connect(&event_runtime, &EventRuntime::channelsChanged, &device,
        [&device, &event_runtime](int channel_a, int channel_b) {
            device.set_event_output(QStringLiteral("A"), channel_a);
            device.set_event_output(QStringLiteral("B"), channel_b);
            device.apply_event_wave(QStringLiteral("A"), event_runtime.engine().frame_wave("A"));
            device.apply_event_wave(QStringLiteral("B"), event_runtime.engine().frame_wave("B"));
        });
    WaveLibrary::instance().initialize();
    WaveBridge wave(&device);
    wave.initialize();
    // 蓝牙链路管理器：不持有任何固定设备，按扫描入口识别出的型号为每台设备建实例
    // （郊狼 / 负鼠可同时在线，各自一份连接状态与电量）；实例以 MAC 为键登记，断开只销毁该 MAC
    BleDeviceManager ble_links;
    CoyoteBleController ble;
    ble.attach_link_manager(&ble_links);
    ble.initialize();
    // 输入设备（爪印 / 灵猫）：实例由链路管理器按 MAC 与型号创建、断开时销毁，
    // 每台设备一份数值桥接 —— 模块名 paw_<MAC后四位> / civet_<MAC后四位>，多台同型号互不覆盖
    QHash<QString, DeviceValueBridge*> input_bridges;
    QObject::connect(&ble_links, &BleDeviceManager::inputLinkCreated, &ble_links,
        [&input_bridges, &event_runtime](InputLink* link) {
            if (link == nullptr) {
                return;
            }
            const DeviceValueBridge::DeviceKind kind = link->type == QStringLiteral("pawprint")
                ? DeviceValueBridge::DeviceKind::PawPrints
                : DeviceValueBridge::DeviceKind::Civet;
            auto* bridge = new DeviceValueBridge(kind, link->module_name.toStdString(),
                link->display_name.toStdString(), link->driver);
            bridge->attach_event_runtime(&event_runtime);
            bridge->initialize();
            input_bridges.insert(link->address, bridge);
        });
    // 断开：注销该实例的数值模块、内置事件声明与引擎运行期状态，再销毁桥接
    QObject::connect(&ble_links, &BleDeviceManager::inputLinkDestroyed, &ble_links,
        [&input_bridges](InputLink* link) {
            if (link == nullptr) {
                return;
            }
            DeviceValueBridge* bridge = input_bridges.take(link->address);
            if (bridge == nullptr) {
                return;
            }
            bridge->shutdown();
            delete bridge;
        });
    // 实验性设备首次连接成功的一次性提示：输入类设备不实现 IOutputDevice，按 MAC 复用同一机制
    QObject::connect(&ble_links, &BleDeviceManager::inputLinkConnected, &device, [&device](InputLink* link) {
        if (link != nullptr) {
            device.notify_experimental_device(link->address);
        }
    });

    // 蓝牙输出链路：链路管理器每建成一台实例就接入设备控制器，销毁时再移出；
    // 默认设备评选（郊狼优先，其次负鼠，同类型先连接者优先）与上下限都按设备进行
    QObject::connect(&ble_links, &BleDeviceManager::linkCreated, &device,
        [&device](IOutputDevice* link) { device.attach_output(link); });
    QObject::connect(&ble_links, &BleDeviceManager::linkDestroyed, &device,
        [&device](IOutputDevice* link) { device.detach_output(link); });

    QObject::connect(&device, &DeviceController::statusMessage, &bridge,
        [&bridge](const QString& message) { bridge.setStatus(message); });
    QObject::connect(&device, &DeviceController::errorOccurred, &bridge,
        [&bridge](const QString& message) { bridge.setStatus(message); });

    // 初始化更新系统
    UpdateManager update_manager;
    UpdateBridge update_bridge(&update_manager);

    QQmlApplicationEngine engine;
    // 插件侧车头图：image://modulebanner/<插件文件名>/<头图文件名>，提供器由引擎接管生命周期
    engine.addImageProvider(QStringLiteral("modulebanner"), new ModuleBannerImageProvider());
    engine.addImageProvider(QStringLiteral("dglabqr"), device.qr_image_provider());
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("device"), &device);
    engine.rootContext()->setContextProperty(QStringLiteral("home"), &home);
    engine.rootContext()->setContextProperty(QStringLiteral("moduleBridge"), &module_bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("eventBridge"), &event_bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("waveBridge"), &wave);
    engine.rootContext()->setContextProperty(QStringLiteral("ble"), &ble);
    engine.rootContext()->setContextProperty(QStringLiteral("ruleGraph"), &rule_graph);
    engine.rootContext()->setContextProperty(QStringLiteral("logBridge"), &log_bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("updateBridge"), &update_bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("tray"), &tray);

    // QML 交互连接集中在 C++ 侧
    UiConnector connector(&bridge, &home, &theme, &device, &module_bridge, &wave, &ble, &rule_graph,
        &log_bridge, &update_bridge);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("Dglab", "MainWindow");
    if (engine.rootObjects().isEmpty()) {
        LOG_MODULE("main", "main", LOG_ERROR, "QML 主界面加载失败");
        return -1;
    }
    connector.attach(engine.rootObjects().first());
    tray.attach(qobject_cast<QQuickWindow*>(engine.rootObjects().first()), connector.mini_window());
    LOG_MODULE("main", "main", LOG_DEBUG, "QML 主界面已加载");

    // 首次运行的一次性提示：Windows 11 默认把新建的托盘图标收进折叠区，应用无法程序化强制显示，
    // 只能引导用户自己把它拖到常显区域；提示过一次后写 app.ui.tray_hint_shown 不再重复。
    if (tray.available() && !config.get_value<bool>("app.ui.tray_hint_shown", false)) {
        config.set_value_with_name<bool>("app.ui.tray_hint_shown", true, "user");
        config.save_all();
        QTimer::singleShot(1500, &bridge, [&bridge]() {
            bridge.showToast(
                QStringLiteral("托盘图标可能收在折叠区：点任务栏「^」展开后把它拖到常显区域"),
                AppBridge::StatusWarning);
        });
    }

    const int exit_code = app.exec();

    return exit_code;
}
