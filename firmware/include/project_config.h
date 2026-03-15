#pragma once

// 项目配置聚合入口（config aggregation）。
// 覆盖优先级：project_local.h（可选） > project_defaults.h。
//
// 参数修改入口（常用）：
// 1) 日常修改请在 `config/project_local.h` 覆盖 macro，不建议直接改默认文件。
// 2) 声音大小相关参数见 `config/project_defaults.h` 的 Speaker 区域。
// 3) 运行时可用串口命令临时改音量：`VOL LOW|MED|HIGH`。

#if __has_include("config/project_local.h")
#include "config/project_local.h"
#endif

#include "config/project_defaults.h"
