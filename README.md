# QtUtilities

能够在 Qt 中使用的一些工具，均以单文件头文件库形式提供（C++17 `inline` 变量 + `inline` 函数，直接 `#include` 即用，无需任何宏）。

---

## 组件列表

### [src/log/log.h](src/log/log.h) — Qt 日志工具

彩色控制台输出 + 按天滚动写入 UTF-8 日志文件 + 过期日志清理，线程安全。

**使用：** 在 `.pro` 中加 `CONFIG += c++17` 和 `DEFINES += QT_MESSAGELOGCONTEXT`，然后直接 `#include "log.h"` 即可。示例见 [examples/log/](examples/log/)。
