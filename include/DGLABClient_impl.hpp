#pragma once

#include "DGLABClient.h"
#include "DebugLog.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QThreadPool>

// ============================================
// 模板函数实现（async_call）
// ============================================

template<typename Callback>
inline void DGLABClient::async_call(const QJsonObject& cmd, int timeout, Callback&& callback) {
    LOG_MODULE("DGLABClient", "async_call", LOG_DEBUG, "在后台线程执行异步调用");
    QThreadPool::globalInstance()->start([this, cmd, timeout, callback = std::forward<Callback>(callback)]() mutable {
        try {
            LOG_MODULE("DGLABClient", "async_call", LOG_DEBUG,
                "正在发送命令: " << DebugLogUtil::remove_newline(QJsonDocument(cmd).toJson().toStdString()));
            py_manager_->call(cmd, [callback = std::move(callback)](const QJsonObject& resp) mutable {
                bool ok = resp["status"].toString() == "ok";
                QString msg = resp["message"].toString();
                callback(ok, msg);
                }, timeout);
        }
        catch (const std::runtime_error& e) {
            emit close_finished(false, QString("运行时错误: ") + e.what());
        }
        catch (const std::exception& e) {
            emit close_finished(false, QString("异常: ") + e.what());
        }
        catch (...) {
            emit close_finished(false, "未知异常");
        }
        });
}
