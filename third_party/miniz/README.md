# miniz（第三方库）

- 来源：https://github.com/richgel999/miniz （tag 3.0.2 的 release 源码包）
- 版本：3.0.2（2026 年引入）
- 许可证：MIT（见同目录 `LICENSE`，原样保留）
- 用途：波形库 zip 批量导出 / 导入的归档读写（deflate 压缩与解压）。Qt 6 没有公开的 zip API，`QZipReader` / `QZipWriter` 属 QtGui 私有头，故内嵌本库，不自己实现 deflate。

## 文件形态

单文件 amalgamation（`miniz.c` + `miniz.h`），按上游 `CMakeLists.txt` 的 `AMALGAMATE_SOURCES` 分支同一套步骤生成：

1. `miniz.h` 依次接 `miniz_common.h` / `miniz_tdef.h` / `miniz_tinfl.h` / `miniz_zip.h`；
2. `miniz.c` 依次接 `miniz_tdef.c` / `miniz_tinfl.c` / `miniz_zip.c`；
3. 删除这些文件互相引用的 `#include "<内部头>.h"` 行；
4. `miniz.h` 前置 `MINIZ_EXPORT` 兜底宏（上游生成路径同样如此：单文件形态不再需要 CMake 生成的 `miniz_export.h`）。

与上游脚本的一处差异：`miniz.c` 顶部保留一行 `#include "miniz.h"`。上游生成物是给「包含式（header-only）」用法准备的，本仓库把 `miniz.c` 当独立编译单元编译，必须自带声明。

除上述生成步骤与该行 include 外，`miniz.c` / `miniz.h` 没有任何本地修改（没有手工改动逻辑，后续也不要手工编辑这两个文件：升级时按上面的步骤重新生成）。

## 使用范围

读写均走内存 API：`mz_zip_writer_init_heap` / `mz_zip_writer_add_mem` / `mz_zip_writer_finalize_heap_archive`，以及 `mz_zip_reader_init_mem` / `mz_zip_reader_file_stat` / `mz_zip_reader_get_filename` / `mz_zip_reader_extract_to_heap`。

## 构建

根 `CMakeLists.txt` 里以 `add_library(miniz STATIC third_party/miniz/miniz.c)` 单独编译（C89 风格 C 源码，需 `enable_language(C)`），主程序与相关测试目标都链接它。
