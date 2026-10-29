/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "AppBridge.h"
#include "AppConfig.h"
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
#include "OpossumBleController.h"
#include "PawPrintsBleController.h"
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

    // 配置初始化
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
    CoyoteBleController ble;
    ble.initialize();
    // 负鼠振动控制器（实验性）：扫描与连接入口复用蓝牙入口对象，写入由负鼠驱动接管
    OpossumBleController opossum;
    opossum.initialize();
    ble.attach_opossum(&opossum);
    // 爪印无线按钮传感器 V1.1（实验性输入设备）：扫描与连接入口复用蓝牙入口对象，写入由爪印驱动接管
    PawPrintsBleController pawprints;
    pawprints.initialize();
    ble.attach_pawprints(&pawprints);
    // 爪印通知 → paw 数值模块，该模块的内置事件声明接进事件运行时（插件声明之外的第二个来源）
    DeviceValueBridge paw_bridge(&pawprints);
    paw_bridge.initialize();
    event_runtime.register_builtin_events(DeviceValueBridge::kModuleName, DeviceValueBridge::builtin_events());
    // 实验性设备首次连接成功的一次性提示：输入类设备不实现 IOutputDevice，按 MAC 复用同一机制
    QObject::connect(&pawprints, &PawPrintsBleController::stateChanged, &device, [&device, &pawprints]() {
        if (pawprints.connected()) {
            device.notify_experimental_device(pawprints.device_address());
        }
    });

    // 蓝牙接入输出链路：已连接的设备参与默认设备评选（郊狼优先，其次负鼠）
    device.attach_output(&ble);
    device.attach_output(&opossum);

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

    return app.exec();
}
