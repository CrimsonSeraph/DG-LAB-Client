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

> **例外**：重载 Qt 的虚函数（如 `paintEvent`、`mousePressEvent`）时，保持 Qt 原有的命名风格，不必强制改为 snake_case。

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

### 5. 注释规范

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

### 6. 其他约定

- 未在本规范中明确提及的内容（如 switch 语句格式、异常处理方式等），可保持个人习惯，后续逐步补充。

## 开发环境

- **Qt 版本**：5.15 / 6.x （建议使用最新稳定版本）
- **编译器**：MSVC / MinGW（支持 C++20 及以上）
- **Python 版本**：3.9+
- **构建工具**：CMake
