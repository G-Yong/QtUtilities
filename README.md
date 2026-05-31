# QtUtilities

能够在 Qt 中使用的一些工具，均以单文件头文件库形式提供（C++17 `inline` 变量 + `inline` 函数，直接 `#include` 即用）。

## 组件列表

| 组件 | 说明 | 示例 |
| --- | --- | --- |
| [src/log.h](src/log.h) | Qt 日志工具：彩色控制台输出、按天写入 UTF-8 日志文件、过期日志清理、qDebug/qInfo/qWarning/qCritical/qFatal/stdout/stderr/printf 捕获。 | [examples/log](examples/log) |
| [src/cv.h](src/cv.h) | OpenCV + Qt 图像互操作工具：Mat/QImage/bytes 互转、中文路径读写、Qt 绘制 Unicode 文本、棋盘图生成。 | [examples/cv](examples/cv) |
| [src/system.h](src/system.h) | 系统/硬件信息工具（仅依赖 Qt Core，跨 Windows/Linux）：随机 UUID 与机器唯一 UUID、主机名/操作系统/内核/架构、CPU 型号与核心数、整机与进程 CPU 使用率（含可持续轮询的监视器类）、物理内存/交换区/进程内存、磁盘容量、系统运行时长、字节与时长格式化。 | [examples/system](examples/system) |