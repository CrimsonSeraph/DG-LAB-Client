#include "DGLABClient.h"
#include "AppConfig.h"
#include "PyExecutorManager.h"
#include "Console.h"
#include "DebugLog.h"
#include <QtWidgets/QApplication>
#include <iostream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    // 直接创建控制台，以便在初始化配置系统时输出日志
    //Console& console = Console::GetInstance();
    //console.Create();
    // 配置初始化
    AppConfig& config = AppConfig::instance();
    std::string config_dir = "./config";
    try {
        if (!config.initialize(config_dir)) {
            DebugLog::Instance().set_log_level("main", LOG_DEBUG);
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
    bool enable_console = config.get_value<bool>("app.debug", false);
    if (enable_console) {
        Console& console = Console::GetInstance();
        if (console.Create()) {
            LOG_MODULE("main", "main", LOG_DEBUG, "控制台已启用");
        }
        else {
            LOG_MODULE("main", "main", LOG_WARN, "控制台启用失败（非 Windows 平台不支持）");
        }
    }

    int debug_log_level = config.get_value<int>("app.log.level", 0);
    DebugLog::Instance().set_all_log_level(debug_log_level);
    bool is_only_type_info = config.get_value<bool>("app.log.only_type_info", false);
    DebugLog::Instance().set_only_type_info(is_only_type_info);

    // 创建窗口
    DGLABClient window;
    std::string app_name = config.get_value<std::string>("app.name", "DG-LAB-Client");
    std::string app_version = config.get_value<std::string>("app.version", "1.0.0");
    window.setWindowTitle(QString::fromStdString(app_name + "[" + app_version) + "]");
    window.show();

    // 初始化 Python 解释器
    py::scoped_interpreter guard{};
    PyExecutorManager manager;

    // 设置 Python 路径
    std::string python_path = config.get_value<std::string>("python.path", "python");
    std::string packages_path = config.get_value<std::string>("python.packages_path", "python/Lib/site-packages");
    py::module sys = py::module::import("sys");

    std::filesystem::path python_dir = std::filesystem::current_path() / python_path;
    if (std::filesystem::exists(python_dir)) {
        sys.attr("path").attr("append")(python_dir.string());
    }
    std::filesystem::path full_packages = std::filesystem::current_path() / packages_path;
    if (std::filesystem::exists(full_packages)) {
        sys.attr("path").attr("append")(full_packages.string());
    }

    //if (!manager.register_executor("WebSocketCore", "DGLabClient", false)) {
    //    std::cerr << "执行器注册失败！";
    //}
    //try {
    //    manager.call_sync<void>("WebSocketCore", "DGLabClient", "set_ws_url", "ws://localhost:9999");

    //    bool is_connect = manager.call_sync<bool>("WebSocketCore", "DGLabClient", "connect");
    //    if (!is_connect) {
    //        std::cerr << "连接失败" ;
    //    }
    //    else {
    //        manager.call_sync<bool>("WebSocketCore", "DGLabClient", "sync_send_strength_operation", 1, 2, 10);
    //        std::string server_address = manager.call_sync<std::string>("WebSocketCore", "DGLabClient", "generate_qr_content");
    //        std::cout << server_address;
    //    }
    //    manager.call_sync<void>("WebSocketCore", "DGLabClient", "sync_close");
    //}
    //catch (const std::exception& e) {
    //    std::cerr << "调用失败: " << e.what() ;
    //}

    return app.exec();
}
