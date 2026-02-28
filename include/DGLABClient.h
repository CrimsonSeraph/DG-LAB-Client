#pragma once

#include "DebugLog.h"
#include "PyExecutorManager.h"

#include <QtWidgets/QWidget>
#include "ui_DGLABClient.h"
#include <QSyntaxHighlighter>


class DGLABClient : public QWidget
{
    Q_OBJECT

public:
    DGLABClient(QWidget* parent = nullptr);
    ~DGLABClient();

    void on_main_first_btn_clicked();
    void on_main_config_btn_clicked();
    void on_main_setting_btn_clicked();
    void on_main_about_btn_clicked();

    void on_start_connect_btn_clicked();
    void on_close_connect_btn_clicked();
    void on_start_btn_clicked();
    void on_close_btn_clicked();

    void change_ui_log_level(LogLevel new_level);

private:
    Ui::DGLABClientClass ui;
    void append_log_message(const QString& message, int level);
    void append_colored_text(QTextEdit* edit, const QString& text);
    QSyntaxHighlighter* logHighlighter = nullptr;

    bool start_connect_btn_loading = false;
    bool close_connect_btn_loading = false;

    bool is_connected = false;

    LogLevel ui_log_level = LOG_DEBUG;
    LogSink qtSink;

signals:
    void connect_finished(bool success, const QString& message);
    void code_content_ready(const QString& content);
    void close_finished(bool success, const QString& message);

private slots:
    void handle_connect_finished(bool success, const QString& msg);
    void handle_code_content_ready(const QString& content);
    void handle_close_finished(bool success, const QString& msg);
    void start_async_connect();
    void close_async_connect();
};

