#pragma once

#include "DebugLog.h"

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

    void change_ui_log_level(LogLevel new_level);

private:
    Ui::DGLABClientClass ui;
    void appendLogMessage(const QString& message, int level);
    void appendColoredText(QTextEdit* edit, const QString& text, const QColor& color);
    QSyntaxHighlighter* logHighlighter = nullptr;

    LogLevel ui_log_level = LOG_DEBUG;
    LogSink qtSink;
};

