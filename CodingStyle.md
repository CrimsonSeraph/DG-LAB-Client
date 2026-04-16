# DG-LAB-Client CodingStyle

基于 Qt 的应用程序，通过 Python 模块扩展功能。  
本项目遵循明确的编码风格规范，以保证代码的可读性与一致性。

## 目录结构

```text
.
├── include/      # C++ 头文件（.h, .hpp 等）
├── src/          # C++ 源文件（.cpp）
├── assets/       # 资源文件（图片、字体、音频等）
├── config/       # 配置文件（.json）
├── qcss/         # Qt 样式表文件（.qss）
└── python/       # Python 脚本及模块
```

## 编码规范

### 1. 文件编码与换行

- **字符编码**：UTF-8 without BOM
- **换行符**：CRLF（Windows 风格）
- **文件末尾**：必须保留一个空行

### 2. 缩进与空白

- **缩进方式**：4 个空格（不使用 Tab）
> 若使用 Tab，必须设置为等同于 4 个空格且最后文件保存时转换为 4 个空格
- **行最大长度**：暂不作严格限制，但建议不超过 120 字符
- **括号风格**：左括号与语句同行（K&R 风格）

```cpp
// 正确示例
if (condition) {
    do_something();
}
```

### 3. 命名规则

| 元素 | 风格 | 示例 |
| - | - | - |
| 类名 | 双驼峰（PascalCase） | `MainWindow`, `DataBase` |
| 变量名 | 小写 + 下划线（snake_case） | `user_name`, `max_count` |
| 函数名 | 小写 + 下划线（snake_case） | `get_data()`, `save_file()` |
| 常量/宏 | 全大写 + 下划线 | `MAX_BUFFER_SIZE` |
| Python 模块 | 双驼峰（PascalCase） | `FileUtils.py` |

> **注意**：在 Python 模块中，文件名使用 PascalCase 以区分于变量和函数，但模块内的函数和变量仍使用 snake_case。
  **例外**：重载 Qt 的虚函数（如 `paintEvent`、`mousePressEvent`）时，保持 Qt 原有的命名风格，不必强制改为 snake_case。

### 4. 导入顺序（C++ 与 Python）

- **顺序规则**：  
  1. 自定义模块/头文件  
  2. 第三方库  
  3. 标准库  

- **内部排序**：每组内按字母顺序排列

#### C++ 示例（`#include`）

```cpp
// 自定义头文件
#include "main_window.h"
#include "utils.h"

// 第三方库（例如 Qt）
#include <QApplication>
#include <QPushButton>

// 标准库
#include <iostream>
#include <vector>
```

#### Python 示例（`import`）

```python
# 自定义模块
from python import helper
from config import settings

# 第三方库
import numpy
import PyQt5

# 标准库
import json
import os
```

### 5. 头文件中的各项定义顺序

类声明中各分区必须按以下顺序排列（若某分区为空，可省略）：

```
public: → protected: → private: → signals: → private slots:
```

#### 5.1 各分区内部成员排列顺序

- **`public` 分区**  
  1. 构造函数 / 析构函数  
  2. 公共接口（普通成员函数）  
  3. 公共成员变量（极少使用）

- **`protected` 分区**  
  1. 重写的虚函数（`override`）  
  2. 其他受保护方法  
  3. 受保护成员变量

- **`private` 分区**  
  1. 常量（`static constexpr` / `const`）  
  2. 成员变量  
  3. 私有辅助函数

- **`signals` 分区**  
  Qt 信号声明（无实现）

- **`private slots` 分区**  
  Qt 私有槽函数声明

#### 5.2 特殊成员的放置位置（按优先级）

- **友元声明**：紧接在 `private:` 标签之后的第一行  
- **静态成员变量**：放在 `private:` 区的最开头（常量之前）  
- **静态成员函数**：放在所属分区（`public` / `private`）的普通成员函数之前  
- **`Q_INVOKABLE` 方法**：放在 `public:` 区，并显式添加 `Q_INVOKABLE` 宏

#### 5.3 模板函数与静态工具函数

- **模板函数**：仅声明在头文件中，定义放入同名的 `类名_impl.h` 文件（同一目录）。  
- **静态工具函数**：不要放在类声明内，应独立到 `_util.h` / `_util.cpp` 文件中。

### 6. 源文件组织规范

- 函数定义的顺序**必须**与头文件中各分区的声明顺序严格一致。  
- 每个分区内，函数定义的顺序也必须与头文件中声明的顺序相同。  
- 若头文件有调整（如成员函数重排），源文件需同步调整定义顺序。

### 7. 函数注释规范

为每个函数添加说明注释，推荐使用 Doxygen 风格：

- 复杂函数使用以下格式：

```cpp
/// @brief 简短功能描述
/// @param param1 参数说明
/// @param param2 参数说明
/// @return 返回值说明
/// @note 注意事项（可选）
int calculate(int param1, double param2);
```

- 简单函数可在函数前一行使用 `//` 进行简短描述：

```cpp
// 检查用户是否已登录
bool is_logged_in();
```

- 若无参数、返回值或特殊说明，对应标签可省略。

### 8. 日志规范（LOG_MODULE 宏）

项目中使用 `LOG_MODULE` 宏输出调试信息。请遵循以下原则：

#### 8.1 添加调试信息的位置

- 函数入口/出口（尤其是复杂逻辑、状态改变、资源分配释放等）
- 关键分支（`if`/`else`、`switch`）和循环开始/结束
- 错误处理路径、异常路径
- 重要变量值的变更前后
- 外部调用（API、系统调用）前后

#### 8.2 减少不必要的调试信息

- 过于频繁调用的函数（如每帧调用的更新函数）中避免输出每次调用，可改为采样或条件输出
- 移除明显冗余的日志（例如连续输出相同信息，或与上下文重复的信息）
- 避免在热点路径（高频循环）中输出日志

#### 8.3 日志级别使用

| 级别 | 用途 |
|------|------|
| `LOG_ERROR` | 严重错误，影响功能 |
| `LOG_WARN`  | 可恢复的异常情况 |
| `LOG_INFO`  | 重要的状态变化、模块初始化/销毁、用户操作等 |
| `LOG_DEBUG` | 详细调试信息，默认可能在 Release 版本禁用 |

> 根据实际情况选择合适的级别，不要滥用 `LOG_INFO` 和 `LOG_ERROR`。

#### 8.4 目标

调试信息充分但不过量，能帮助定位问题，不影响性能或淹没关键信息。

### 9. 注释规范（基本）

- **单行注释（独占一行）**：放在代码块前面，与代码块开头保持相同缩进级别。  
  `//` 后跟一个空格，再写注释内容。

```cpp
// 检查用户是否已登录
if (is_logged_in) {
    show_main_page();
}
```

- **行尾注释**：与代码之间**间隔一个 Tab**（不是多个空格），`//` 后跟一个空格。

```cpp
int count = 0;  // 当前重试次数
bool is_ready = false;  // 是否准备就绪
```

- **其他注释形式**（块注释、文档注释等）不作强制要求，可自由使用。

### 10. 其他约定

- 头文件使用 `#pragma once` 进行防止重复包含。
- 未在本规范中明确提及的内容（如 switch 语句格式、异常处理方式等），可保持个人习惯，后续逐步补充。
- 模板函数定义已分离到 `_impl.h`。
- 静态工具函数移出类声明，放入独立 `_util`。

## 开发环境

- **Qt 版本**：5.15 / 6.x （建议使用最新稳定版本）
- **编译器**：MSVC / MinGW（支持 C++20 及以上）
- **Python 版本**：3.9+
- **构建工具**：CMake
