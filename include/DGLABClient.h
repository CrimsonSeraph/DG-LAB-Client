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
    void appendLogMessage(const QString& message, int level);
    void appendColoredText(QTextEdit* edit, const QString& text);
    QSyntaxHighlighter* logHighlighter = nullptr;

    bool start_connect_btn_loading = false;

    bool executor_is_register = false;

    LogLevel ui_log_level = LOG_DEBUG;
    LogSink qtSink;
};

