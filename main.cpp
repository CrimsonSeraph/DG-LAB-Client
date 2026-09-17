/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "AppBridge.h"
#include "AppConfig.h"
#include "Console.h"
#include "DebugLog.h"
#include "DeviceController.h"
#include "HomeBridge.h"
#include "ModuleBridge.h"
#include "ModuleManager.h"
#include "RuleManager.h"
#include "ThemeManager.h"
#include "UiConnector.h"
#include "WaveBridge.h"
#include "WaveLibrary.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickStyle>

#include <iostream>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    // 统一使用 Basic 样式：界面外观完全由 QML 的样式令牌控制（不叠加平台样式）
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // 直接创建控制台，以便在初始化配置系统时输出日志
    Console& console = Console::get_instance();
    console.create();

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
    WaveLibrary::instance().initialize();
    WaveBridge wave(&device);
    wave.initialize();

    QObject::connect(&device, &DeviceController::statusMessage, &bridge,
        [&bridge](const QString& message) { bridge.setStatus(message); });
    QObject::connect(&device, &DeviceController::errorOccurred, &bridge,
        [&bridge](const QString& message) { bridge.setStatus(message); });

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("dglabqr"), device.qr_image_provider());
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("device"), &device);
    engine.rootContext()->setContextProperty(QStringLiteral("home"), &home);
    engine.rootContext()->setContextProperty(QStringLiteral("moduleBridge"), &module_bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("waveBridge"), &wave);

    // QML 交互连接集中在 C++ 侧
    UiConnector connector(&bridge, &home, &theme, &device, &module_bridge, &wave);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("Dglab", "MainWindow");
    if (engine.rootObjects().isEmpty()) {
        LOG_MODULE("main", "main", LOG_ERROR, "QML 主界面加载失败");
        return -1;
    }
    connector.attach(engine.rootObjects().first());
    LOG_MODULE("main", "main", LOG_DEBUG, "QML 主界面已加载");

    return app.exec();
}
