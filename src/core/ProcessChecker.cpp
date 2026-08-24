#include "ProcessChecker.h"

#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>

bool ProcessChecker::isRunning(const QString& processName, bool caseSensitive) {
    if (processName.isEmpty())
        return false;

    QProcess process;
    QString output;
    bool success = false;

#ifdef Q_OS_WIN
    // Windows: 使用 tasklist 输出 CSV 格式，便于解析
    QStringList args;
    args << "/FO" << "CSV" << "/NH" << "/FI" << QString("IMAGENAME eq %1").arg(processName);
    process.start("tasklist", args);
    if (!process.waitForFinished(3000)) {
        qWarning() << "ProcessChecker: tasklist timed out.";
        return false;
    }
    if (process.exitCode() != 0) {
        qWarning() << "ProcessChecker: tasklist failed with code" << process.exitCode();
        return false;
    }
    output = process.readAllStandardOutput();

    // 解析CSV：每行格式 "进程名","PID","会话名","会话#","内存使用"
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        // 简单CSV解析：取第一个字段，去掉首尾引号
        if (line.startsWith('"')) {
            int firstQuote = line.indexOf('"', 1);
            if (firstQuote > 0) {
                QString name = line.mid(1, firstQuote - 1);
                if (caseSensitive) {
                    if (name == processName)
                        return true;
                }
                else {
                    if (name.compare(processName, Qt::CaseInsensitive) == 0)
                        return true;
                }
            }
        }
    }

#else
    // Unix-like (macOS / Linux): 使用 ps -e -o comm= 获取所有进程的命令名
    process.start("ps", QStringList() << "-e" << "-o" << "comm=");
    if (!process.waitForFinished(3000)) {
        qWarning() << "ProcessChecker: ps timed out.";
        return false;
    }
    if (process.exitCode() != 0) {
        qWarning() << "ProcessChecker: ps failed with code" << process.exitCode();
        return false;
    }
    output = process.readAllStandardOutput();

    // 每行一个进程名
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QString name = line.trimmed();
        if (caseSensitive) {
            if (name == processName)
                return true;
        }
        else {
            if (name.compare(processName, Qt::CaseInsensitive) == 0)
                return true;
        }
    }

#endif

    return false;
}
