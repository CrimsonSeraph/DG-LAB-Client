#include "Console.h"
#include "DebugLog.h"

Console::Console() : m_isCreated(false) {
    // 构造函数不自动创建控制台
}

Console::~Console() {
    Destroy();
}

Console& Console::GetInstance() {
    static Console instance;  // 局部静态变量
    return instance;
}

bool Console::Create() {
    if (m_isCreated) {
        return true;
    }

#ifdef _WIN32
    m_isCreated = CreateDebugConsole();
#else
    LOG_MODULE("Console", "Create", LOG_WARN, "当前操作系统不支持控制台输出！");
    m_isCreated = false;
#endif
    return m_isCreated;
}

void Console::Destroy() {
#ifdef _WIN32
    if (m_isCreated) {
        FreeConsole();
        m_isCreated = false;
    }
#else
    m_isCreated = false;
#endif
}

#ifdef _WIN32
bool Console::CreateDebugConsole() {
    // 分配控制台
    if (!AllocConsole()) {
        DWORD error = GetLastError();
        // 如果已经分配了控制台，这也是成功的
        if (error == ERROR_ACCESS_DENIED) {
            // 控制台可能已经存在（比如从命令行启动）
            // 我们可以附加到现有的控制台
            if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
                return false;
            }
        }
        else {
            return false;
        }
    }

    // 获取标准输出句柄
    HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hError = GetStdHandle(STD_ERROR_HANDLE);

    if (hOutput == INVALID_HANDLE_VALUE ||
        hInput == INVALID_HANDLE_VALUE ||
        hError == INVALID_HANDLE_VALUE) {
        return false;
    }

    // 设置控制台代码页为UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 重定向标准流到控制台
    FILE* fp;

    // 重定向
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);

    // 同步C++标准流与C流
    std::ios::sync_with_stdio(true);

    // 清除流状态
    std::cout.clear();
    std::cerr.clear();
    std::cin.clear();

    // 设置控制台字体
    CONSOLE_FONT_INFOEX cf = { 0 };
    cf.cbSize = sizeof(cf);
    cf.dwFontSize.Y = 14;  // 字体大小

    // 尝试设置几种常见的支持Unicode的字体
    const wchar_t* fontNames[] = {
        L"Consolas",
        L"Lucida Console",
        L"DejaVu Sans Mono",
        L"MS Gothic"
    };

    for (const wchar_t* fontName : fontNames) {
        wcscpy_s(cf.FaceName, fontName);
        if (SetCurrentConsoleFontEx(hOutput, FALSE, &cf)) {
            break;
        }
    }

    // 启用虚拟终端处理
    DWORD dwMode = 0;
    if (GetConsoleMode(hOutput, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOutput, dwMode);
    }

    // 设置控制台标题
    SetConsoleTitleW(L"Debug Console");

    return true;
}
#endif
