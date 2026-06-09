# QtUtilities

能够在 Qt 中使用的一些工具，均以单文件头文件库形式提供（C++17 `inline` 变量 + `inline` 函数，直接 `#include` 即用）。

## 组件列表

| 组件 | 说明 | 示例 |
| --- | --- | --- |
| [src/log.h](src/log.h) | Qt 日志工具：彩色控制台输出、按天写入 UTF-8 日志文件、过期日志清理、qDebug/qInfo/qWarning/qCritical/qFatal/stdout/stderr/printf 捕获。 | [examples/log](examples/log) |
| [src/cv.h](src/cv.h) | OpenCV + Qt 图像互操作工具：Mat/QImage/bytes 互转、中文路径读写、Qt 绘制 Unicode 文本、棋盘图生成。 | [examples/cv](examples/cv) |
| [src/system.h](src/system.h) | 系统/硬件信息工具（仅依赖 Qt Core，跨 Windows/Linux）：随机 UUID 与机器唯一 UUID、主机名/操作系统/内核/架构、CPU 型号与核心数、整机与进程 CPU 使用率（含可持续轮询的监视器类）、物理内存/交换区/进程内存、磁盘容量、系统运行时长、字节与时长格式化。 | [examples/system](examples/system) |
| [src/file.h](src/file.h) | 文件/目录工具（仅依赖 Qt Core）：UTF-8 文本与二进制读写（写入原子化）、追加、复制/移动/删除、递归创建/删除目录、递归列举文件与子目录、文件大小、文件哈希、路径辅助函数，全程支持中文路径。 | [examples/file](examples/file) |
| [src/crypto.h](src/crypto.h) | 哈希与编码工具（仅依赖 Qt Core）：MD5/SHA1/SHA256/SHA512、HMAC-SHA256、标准与 URL 安全 Base64、Hex 编解码、XOR 简易混淆，均提供字符串与字节数组重载。 | [examples/crypto](examples/crypto) |
| [src/dump.h](src/dump.h) | 崩溃 minidump 保存（Windows 跨平台兼容）：安装全局异常/CRT/SIGABRT 处理器，崩溃时自动写出 .dmp 文件；支持阻止其他模块覆盖异常处理器；非 Windows 平台安全 no-op。 | [examples/dump](examples/dump) |
| [src/timer.h](src/timer.h) | 计时/性能分析工具（仅依赖 Qt Core）：高精度秒表 Stopwatch、RAII 作用域计时 ScopedTimer、一行计时 measureMs、平滑帧率计数 FpsCounter、节流闸 RateLimiter、时长格式化。 | [examples/timer](examples/timer) |