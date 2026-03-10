# PROJECT MEMORY - ESP32 AI Health Assistant

更新时间：2026-03-10（新 MLX90614 已验通并切回主温度链路）

## 1. 项目定位（Project Scope）
- 项目名：ESP32 AI Health Assistant（基于 ESP32-S3 的 AI 智能健康助手系统）
- 类型：本科毕业设计（计算机科学与技术）
- 目标：构建便携式健康监测设备，支持 Heart Rate / SpO2 / Temperature / Fall Detection / OLED Display / AI Voice / Alarm
- 场景：老人监护、日常健康监测、户外运动健康管理

## 2. 当前开发阶段（Current Stage）
- 最小系统已扩展到 3 模块
- 当前硬件：ESP32-S3 + MAX30102 + SSD1306 OLED + MLX90614
- 当前功能：
  - I2C 初始化（GPIO8/GPIO9）
  - OLED 显示（Heart Rate / SpO2 / Temperature）
  - MAX30102 初始化与心率血氧读取
  - Heart Rate 显示策略：`实时值 + 稳定值融合`，短时丢拍不立即归零
  - 温度显示默认来源：MLX90614（失败时自动回退 MAX30102 die temperature）
  - MLX90614 当前默认开启初始化（地址自动探测）
  - 串口调试输出（115200）
  - 串口温度来源标识：`TempSrc=MAX30102_DIE / MLX90614 / NONE`
  - UI 刷新周期约 `250ms`
- 显示优先级：稳定 Heart Rate/SpO2 的同时补充 Temperature

## 3. 主控与开发板（Main Board）
- 开发板：YD-ESP32-S3-COREBOARD V1.4
- 模组：ESP32-S3-WROOM-1（资料中出现 N8R8/N16R8 版本，实际以板载模组丝印为准）
- SoC 特性：Dual-core Xtensa LX7，最高 240MHz，Wi-Fi + Bluetooth
- 板载特性（来自原理图/图片）：
  - USB Type-C（下载/供电/串口）
  - USB-UART 芯片：CH343P
  - 电源芯片：CJ6107A33GW（5V -> 3.3V）
  - RGB LED：WS2812B（GPIO48）
  - 按键：BOOT(GPIO0)、RST(CHIP_PU)
  - 双排针 2x22 Pin，常用 GPIO0~21、35~42、45~48

## 4. 当前最小系统接线（Confirmed Wiring）
- I2C 总线：
  - SDA = GPIO8
  - SCL = GPIO9
  - 初始化：Wire.begin(8, 9)
- I2C 地址：
  - OLED(SSD1306) = 0x3C
  - MAX30102 = 0x57
  - MLX90614 = 0x5A
- MAX30102：
  - VIN -> 3.3V
  - GND -> GND
  - SDA -> GPIO8
  - SCL -> GPIO9
  - INT -> 不连接（当前阶段）
- OLED（0.96", 128x64, SSD1306, I2C）：
  - VCC -> 3.3V
  - GND -> GND
  - SDA -> GPIO8
  - SCL -> GPIO9
- MLX90614（I2C）：
  - VCC -> 3.3V
  - GND -> GND
  - SDA -> GPIO8
  - SCL -> GPIO9
  - 与 OLED / MAX30102 共用同一 I2C 总线

## 5. 完整系统规划模块（Planned Modules）
- I2C：MAX30102、MLX90614、MPU6050、SSD1306
- I2S：INMP441（Mic）、MAX98357A（Audio DAC）
  - 建议引脚：BCLK=GPIO4、LRCLK=GPIO5、DIN=GPIO6、DOUT=GPIO7
- GPIO：Buzzer（建议 GPIO18）
- 电源：18650 + TP4056 + Boost

## 6. 软件栈与工程约定（Software Stack）
- 开发环境：Trae IDE（基于 VS Code）
- 工程方式：PlatformIO + Arduino Framework（无需 Arduino IDE）
- 语言：C++
- 当前必需库：Wire / Adafruit_GFX / Adafruit_SSD1306 / SparkFun_MAX3010x
- 当前可选库：Adafruit_MLX90614（保留用于后续恢复外置测温）

## 7. 数据与AI扩展（Future AI/Data）
- 后续上报示例：
```json
{
  "heart_rate": 72,
  "spo2": 98,
  "temperature": 36.6
}
```
- AI 返回健康建议；后续加入语音查询与语音反馈

## 8. 资料索引（Sources）
- ESP32-S3-inch.pdf
- ESP32-S3-Metric.pdf
- YD-ESP32-S3-SCH-V1.4.pdf
- 开题报告.docx
- 说明：开题报告提取文本存在编码噪声，已以用户确认信息为准固化关键事实。

## 9. 当前决策（Locked Decisions）
- 创建目录：ESP32-AI-Health-Assistant
- 文档风格：中文为主 + 关键英文关键词
- 板卡目标：PlatformIO 使用 esp32-s3-devkitc-1
- 最小系统 MAX30102 INT 不接
- 当前演示版温度策略：默认使用 MLX90614；失败时回退到 MAX30102 die temperature

## 10. 2026-03-06 调试记录（Bring-up Log）
- 编译/烧录链路已打通：
  - `python -m platformio run`
  - `python -m platformio run -t upload`
  - `python -m platformio device monitor -b 115200 -p COM3`
- 设备识别正常：I2C 可稳定扫到 `0x3C`（OLED）与 `0x57`（MAX30102）。
- 当前固件算法状态（旧版）：
  - Heart Rate：`checkForBeat` 逐拍检测。
  - SpO2：SparkFun `maxim_heart_rate_and_oxygen_saturation`。
  - 稳定性措施：Median + EWMA + jump limit + valid/invalid streak gate + timeout drop。
  - 校准流程：手指放置后进入 `Calibration Mode` 建立 perfusion baseline。
  - I2C 速率固定 `100kHz`（提升杜邦线场景稳定性）。
- 当前测试结论：
  - 硬件已验证正常：诊断固件可稳定扫到 `0x3C` 与 `0x57`，OLED 自检和 MAX30102 自检都通过。
  - 旧版测量固件的主要问题不是 I2C，而是 `PPG` 波形质量不足，导致 `Quality` 长期为 `POOR`、`HRValid` 易掉。
  - 因此算法优化方向从“继续调 SparkFun 示例参数”切换到“官方参考设计 / RF 风格的质量门控路线”。
- 操作建议（已验证有效）：
  - 传感器与手指使用橡皮筋轻固定，避免手按与晃动。
  - 保持 30~60 秒静止再观察数值。
  - 避免强光直射，手指偏冷时先预热。

## 11. 2026-03-07 算法升级实验记录（Algorithm Experiment Log）
- 曾尝试切换到 `MAXREFDES117 / MAX30102_by_RF` 风格：
  - `25 sps`
  - `4 秒窗口`
  - `autocorrelation + Pearson correlation + PI` 质量门控
  - `LED amplitude auto-tune`
- 实测结论：
  - 对当前这块 MAX30102 模块、接线方式和佩戴方式不适配
  - 串口中会出现候选值（例如 `CandHR=150` / `CandSpO2≈84`），但主结果长期被判为无效
  - 说明“窗口候选值不稳定 + 质量门控过于敏感”，最终演示效果反而差于旧版
- 当前主线决策：
  - 放弃 RF 风格实验版作为主固件
  - 恢复到之前已实测能正常出心率/血氧的稳定版：
    - Heart Rate：`checkForBeat`
    - SpO2：SparkFun `maxim_heart_rate_and_oxygen_saturation`
    - 稳定性策略：`Calibration + Median + EWMA + jump limit + valid/invalid streak`
  - OLED 继续使用“无效显示 `-- BPM / --%`”的策略
- 工程结论：
  - 对当前毕设阶段，优先选“能稳定演示”的实现，而不是继续在更复杂算法上深挖
  - 参考算法保留在文档与 `reference-projects` 中，后续可在机械固定/遮光优化后再重新评估

## 12. 参考实现与来源（Reference Implementations）
- 官方参考设计：
  - `MAXREFDES117`（Maxim/Analog Devices）
  - 页面：<https://www.analog.com/en/resources/reference-designs/maxrefdes117.html>
- 社区高质量实现：
  - `aromring/MAX30102_by_RF`
  - 页面：<https://github.com/aromring/MAX30102_by_RF>
  - 关键参数参考：
    - `FS = 25`
    - `BUFFER_SIZE = 100`
    - `ST = 4`
    - `min_autocorrelation_ratio = 0.5`
    - `min_pearson_correlation = 0.8`
- 官方驱动与基线示例：
  - SparkFun MAX3010x Library
  - 页面：<https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library>

## 13. 参考资料归档约定（Reference Projects Folder）
- 项目根目录新增英文文件夹：`reference-projects`
- 用途：
  - 存放后续获取的外部参考项目、算法笔记、来源链接
  - 作为“参考实现归档区”，避免散落在主工程目录

## 14. 下一步优化建议（Planned Next）
- 当前主线：保持“可稳定演示”的心率/血氧方案，不再进行大改算法。
- 温度链路：
  - 当前演示使用 `MAX30102 die temperature`（趋势展示）。
  - 若后续要恢复体表测温，再单独排查 `GY-906(MLX90614)` 的供电/接线/模块一致性。
- 结构优化优先级（影响效果更大）：
  - 机械固定（手指 + 传感器）
  - 遮光处理
  - `MPU6050 motion gating`

## 15. 2026-03-07 最新实测结论（Current Stable Conclusion）
- 已回退到“此前实际测出有效值”的稳定版固件，当前不再继续修改 MAX30102 代码。
- 最新串口实测表明：
  - 当手指稳定放置、保持不动时，Heart Rate 可以稳定在合理区间（示例约 `68~75 BPM`，后续也出现过 `78.8 BPM`）。
  - `SpO2` 在稳定阶段可出现有效值（示例 `98.2%`、`98.9%`、`92.0%` 等），但比 Heart Rate 更容易因质量判定掉回无效。
  - `Quality` 经常显示 `POOR`，但这并不等于“完全不可用”；在当前模块和接线条件下，只要手指固定，仍可获得可演示的有效结果。
- 当前工程判断：
  - 最大影响因素是“手指放置稳定性 / 机械固定 / 遮光”，而不是代码主流程错误。
  - 因此 MAX30102 当前版本先收口，后续如需继续优化，优先从固定方式和结构设计入手，而不是继续大改算法。
- 当前锁定策略：
  - 代码暂不再修改。
  - 明天开始优先推进新模块接入（建议按原计划进入 `MLX90614`）。

## 16. 2026-03-07 MLX90614 接入记录（MLX90614 Integration Log）
- 接入方式：
  - 复用现有 I2C 总线（`SDA=GPIO8`, `SCL=GPIO9`）
  - 设备地址默认 `0x5A`
- 固件改动：
  - 新增库：`Adafruit_MLX90614`
  - 新增初始化：`initMlx90614()`
  - 新增读取：`updateTemperature()`，周期约 `1s`
  - OLED 底部行改为：`SpO2 + Temp(object C)`
  - 串口新增输出：`TempObj / TempAmb`
  - 新增健壮性：
    - 初始化改为地址自动探测（`0x5A/0x5B/0x5C/0x5D`）
    - MLX 失败不再阻塞主程序
    - 每 `5s` 自动重试一次，并在重试时临时降到 `50kHz` I2C
- 集成原则：
  - 尽量不改动已稳定的 MAX30102 主流程
  - 体温模块独立读取，失败不影响心率血氧主逻辑

## 17. 2026-03-07 温度通道临时切换（MAX30102 Die Temp Fallback）
- 背景：
  - 当前 GY-906（MLX90614）在现有接线下持续出现 I2C 扫描/初始化失败，短期内影响联调进度。
- 当前临时策略（用于毕设演示推进）：
  - 固件默认关闭 MLX90614 初始化（`kEnableMlx90614 = false`）。
  - 温度显示改为读取 MAX30102 内置温度通道（die temperature）。
  - 串口新增 `TempSrc` 字段用于标识来源（`MAX30102_DIE / MLX90614 / NONE`）。
- 使用说明：
  - 该温度是 MAX30102 芯片内部温度，适合作为设备温度趋势/演示值。
  - 不作为严格体温测量结论；如需体表温度请恢复外置温度传感器方案（如 MLX90614/接触式温度探头）。

## 18. 2026-03-07 本地备份与心率显示优化（Realtime + Stability）
- 备份动作（改代码前完成）：
  - 备份根目录：`D:\Projects\ESP32 Projects\PROJECT_BACKUPS`
  - 项目备份目录：`D:\Projects\ESP32 Projects\PROJECT_BACKUPS\ESP32-AI-Health-Assistant`
  - 本次快照：`2026-03-07_19-22-44_pre-hr-realtime-tuning`
  - 元数据文件：
    - `BACKUP_INFO.md`（功能状态、触发原因、时间、路径）
    - `KEY_FILE_HASHES.txt`（关键文件 SHA256）
    - `BACKUP_INDEX.md`（快照索引）
- 心率优化目标：
  - 提升“观感实时性”，减少“显示 0 后很久才恢复”的现象。
  - 同时保持数值合理，避免明显离谱跳变。
- 代码侧改动（方案B第一版）：
  - UI 刷新从 `1000ms` 调整为 `250ms`。
  - 心率新增双通道状态：
    - `heartRateRealtimeBpm`（逐拍更新）
    - `heartRateDisplayBpm`（显示用平滑值）
  - 新增短时保留机制：
    - `kFingerLostDebounceMs = 1200`（短时掉手不立刻判定无手指）
    - `kHrDisplayHoldMs = 8000`（短时丢拍保留最近显示值）
  - 新增逐拍限幅：
    - `kHrRealtimeMaxJump = 10.0`（抑制离谱突变）
  - 校准时长缩短：
    - `kCalibrationDurationMs` 从 `12000` 降到 `5000`
- 当前预期效果：
  - 心率数字刷新更平滑、更“有实时感”。
  - 质量短时波动时不容易立刻回零。

## 19. 2026-03-08 串口输出精简对齐（Logging Simplification Alignment）
- 当前阶段结论：
  - 先保持现有功能路线不扩展，优先提高演示可读性。
  - 串口输出从“调试信息为主”切换为“演示信息为主”。
- 已对齐的串口字段（演示模式）：
  - `HR`：心率主显示值（BPM，平滑后）
  - `SpO2`：血氧（%）
  - `Temp`：温度（`C`）
  - `Finger`：是否检测到手指（`Y/N`）
  - `Qual`：信号质量（`GOOD/FAIR/POOR`）
  - `State`：状态（`WAIT/CAL/RUN`）
  - `Src`：温度来源（`MAX30102_DIE/MLX90614`）
- 可选保留字段（仅调试时打开）：
  - `PI`（Perfusion Index，灌注指数）
- 说明：
  - 本条为“显示与字段对齐”记忆，代码实现可在下一步单独提交，避免一次改动过多影响联调。

## 20. 2026-03-08 GY-906(MLX90614) 硬件诊断记录（Permanent Hardware Diagnosis）
- 事件经过：
  - 调试过程中曾发生一次误接线：
    - `MLX GND` 误接 `3.3V`
    - `MLX SCL` 误接 `GND`
  - 当时模块出现“明显发热”，随后已立即断电。
- 后续最小化诊断方式：
  - 使用独立测试固件，仅连接 `GY-906 + OLED`，不改主程序。
  - 独立测试环境：`mlx90614-oled-test`
  - 测试功能包括：
    - I2C 扫描（`20k / 50k / 100k`）
    - `0x5A / 0x5B / 0x5C / 0x5D` 地址探测
    - MLX 原始寄存器读取
    - OLED 状态显示
- 串口诊断结果：
  - OLED 地址 `0x3C` 可稳定扫到。
  - MLX 在 `20k / 50k / 100k` 下均表现为：
    - `0x5A -> ADDR_NACK`
    - `0x5B -> ADDR_NACK`
    - `0x5C -> ADDR_NACK`
    - `0x5D -> ADDR_NACK`
  - 结论：MLX 模块未在 I2C 总线上应答。
- 万用表与供电检查结果：
  - ESP32 板上 `3.3V -> GND` 实测约 `3.3V`，正常。
  - GY-906 在仅接 `3.3V / GND / SDA / SCL` 时“不再发热”。
  - 模块断电状态下：
    - `VIN -> GND`：蜂鸣不响，`20kΩ` 档显示 `0.L`
    - `SCL -> GND`：蜂鸣不响，`20kΩ` 档显示 `0.L`
    - `SDA -> GND`：蜂鸣不响，`20kΩ` 档显示 `0.L`
  - 上述结果说明：未测出明显短路，但这不能证明模块一定完好。
- 当前工程判断（锁定结论）：
  - 不是 ESP32 主控代码问题。
  - 不是当前 I2C 主总线问题（OLED 已验证正常）。
  - 当前这块 `GY-906(MLX90614)` 大概率处于以下两种状态之一：
    - 已因误接导致 I2C/芯片内部功能异常，表现为“不发热但无 ACK”
    - 模块本体/兼容芯片/焊接存在问题，导致始终不在线
  - 在当前毕设周期内，不再将这块 GY-906 作为主线调试对象继续投入时间。
- 后续策略：
  - 温度演示主线继续使用 `MAX30102 die temperature`。
  - 若必须恢复红外测温，优先直接更换新的 `MLX90614/GY-906` 模块，再复用现有独立测试固件验证。

## 21. 2026-03-08 跌倒检测与本地报警已并回主程序（MPU6050 + MAX98357A Integrated）
- 回并范围：
  - 在 `firmware/src/main.cpp` 中合并 `MPU6050` 跌倒检测与 `MAX98357A` 本地报警。
  - 原有 `MAX30102` 心率/血氧主流程保持不变，尽量减少联调风险。
- 当前主程序新增能力：
  - `MPU6050` 自动探测 `0x68 / 0x69`
  - 周期读取三轴加速度与陀螺仪
  - 跌倒状态机：`NORMAL / FREEFALL / IMPACT / ALERT / MONITOR`
  - `MAX98357A` 报警音输出（I2S）
  - OLED 显示增加跌倒状态
  - 串口输出增加 `Fall / M / Gmax / MPU / SPK`
- 当前并回后的判定阈值：
  - `FREEFALL`：`M < 0.6g`
  - `IMPACT`：`M > 1.8g` 且 `Gmax > 60 dps`
  - `STILL`：`M` 位于 `0.85~1.15g` 且 `Gmax < 25 dps`
- 报警策略：
  - 连续报警约 `12 秒`
  - 随后间歇提醒约 `30 秒`
  - 无按钮场景下自动收敛，不做无限持续报警
- 工程配置补充：
  - `firmware/platformio.ini` 已给主环境增加 `build_src_filter`
  - 目的是将独立 test 入口与主程序入口隔离，避免重复定义 `setup()/loop()`
- 当前编译状态：
  - `python -m platformio run -e esp32-s3-devkitc-1`
  - 结果：通过
- 当前阶段结论：
  - 项目主程序已具备“生命体征显示 + 跌倒检测 + 本地声音报警”三条主线
  - `MLX90614` 已在新模块上验通并并回主链路，当前作为默认温度来源（保留 MAX30102 温度回退）

## 22. 2026-03-10 新 MLX90614 验证与温度主链路切换（New MLX Validation + Mainline Switch）
- 新购 `MLX90614/GY-906` 已通过独立测试验证：
  - I2C 扫描可见 `0x5A`
  - 库初始化成功：`MLX90614 init OK at 0x5A`
  - 连续读取正常：`Obj/Amb` 温度可稳定输出
- 为快速诊断新增独立测试入口：
  - `firmware/src/mlx90614_quick_test.cpp`
  - `platformio.ini` 新增环境：`mlx90614-quick-test`
- 主程序温度链路已切换：
  - `kEnableMlx90614 = true`
  - 温度优先 `MLX90614`，失败自动回退 `MAX30102 die`
  - 串口 `TempSrc` 可识别当前来源（`MLX90614` 或 `MAX30102_DIE`）

## 23. 2026-03-10 演示界面与稳定性优化（Demo UI + Stability Tuning）
- OLED 显示重排为四块核心数据，去掉冗余文字，降低重叠风险：
  - 上半区：`HR`、`O2`
  - 下半区：`TMP`、`FALL`
  - `FALL` 短码：`NRM/DRP/HIT/ALR/MON`
- 串口输出切换为低频简洁模式（约 `1s` 一行）：
  - `HR=... O2=... T=... F=... Q=... S=... Src=...`
  - `O2` 显示增加 `%`
- 为提高演示阶段“出数率与连续性”，心率血氧门控已适度放宽：
  - `kMinPerfusionIndex: 0.002 -> 0.0012`
  - `kValidStreakRequired: 2 -> 1`
  - `kInvalidStreakDrop: 4 -> 8`
  - `kMaxSpo2JumpPerUpdate: 3.0 -> 4.0`
  - `kMaxSpo2ReacquireJump: 2.5 -> 4.0`
  - `kBeatTimeoutMs: 9000 -> 12000`
  - 新增 `kSpo2DisplayHoldMs = 15000`（减少 `SpO2` 短时掉 `--`）
- 喇叭测试补充：
  - `max98357_speaker_test.cpp` 已增加低音量“悄悄话”参数用于连线确认（`amplitude=0.03`）

## 24. 2026-03-10 心率血氧策略回退（Rollback to Fast Realtime Profile）
- 背景：
  - 在后续“平滑/门控强化”版本中，出现了 `HR` 可显示但 `SpO2` 长时间 `--` 的现象。
  - 用户实测反馈该版本观感不如前一版，且出现 `HR` 偶发偏高（如 `100+`）的问题。
- 本次主线决策：
  - 回退到“更快实时出值”版本作为当前主线，优先保证演示稳定与连续出值。
- 回退后保留的关键改动：
  - `kCalibrationDurationMs = 3000`
  - `kAlgoStepSamples = 25`
  - `kCalibrationMinWindows = 1`
  - 校准结束后不清空测量窗口（避免首个有效值延后）
  - 保留 `algoHeartRate` 回退通道（逐拍暂不新鲜时用于维持 `HR`）
- 回退后移除的后续增强项：
  - `SpO2` 额外显示通道（`spo2DisplayPercent`）
  - 基于 `piRatio` 的额外门控与自适应阈值
  - `SpO2` 非对称涨跌限幅/超长超时等“强化平滑”策略
- 当前编译状态：
  - `python -m platformio run -e esp32-s3-devkitc-1`
  - 结果：通过
