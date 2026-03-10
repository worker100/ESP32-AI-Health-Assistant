# FIRMWARE TEST MEMORY - ESP32 AI Health Assistant

更新时间：2026-03-08（MPU6050 + MAX98357 跌倒报警测试已打通）

## 1. 作用范围
- 本文件只记录 `firmware` 目录下的独立测试环境。
- 主项目长期事实仍以 `docs/PROJECT_MEMORY.md` 为准。
- 目标：让后续模型或开发者快速知道“有哪些 test 程序可直接刷、接线怎么接、目前验证到哪一步”。

## 2. 测试环境清单（PlatformIO Envs）
- 主环境：
  - `esp32-s3-devkitc-1`
  - 用途：主程序/主固件
- 独立测试环境：
  - `mlx90614-oled-test`
  - `mpu6050-oled-test`
  - `max98357-speaker-test`

## 3. 固定硬件基础
- 开发板：ESP32-S3（PlatformIO board: `esp32-s3-devkitc-1`）
- 公共 I2C 引脚：
  - `SDA = GPIO8`
  - `SCL = GPIO9`
- OLED 地址：
  - `0x3C`
- 测试串口：
  - 当前机器实测监视口为 `COM4`
  - `python -m platformio device list` 结果显示为 `USB 串行设备 (COM4)`

## 4. MLX90614 测试环境（`mlx90614-oled-test`）
- 入口文件：
  - `src/mlx90614_oled_test.cpp`
- 用途：
  - 只测试 `GY-906(MLX90614) + OLED`
  - 不改主程序
- 功能：
  - I2C 扫描（`20k / 50k / 100k`）
  - 地址探测（`0x5A / 0x5B / 0x5C / 0x5D`）
  - 原始寄存器读取
  - OLED 状态显示
- 当前锁定结果：
  - OLED 正常
  - MLX 全部 `ADDR_NACK`
  - 当前这块 `GY-906` 不再作为主线调试件继续投入时间

## 5. MPU6050 测试环境（`mpu6050-oled-test`）
- 入口文件：
  - `src/mpu6050_oled_test.cpp`
- 用途：
  - 测试 `MPU6050 + OLED + MAX98357A`
  - 在独立环境里验证跌倒检测和本地报警
- 接线（当前测试通过）：
  - OLED：
    - `SDA -> GPIO8`
    - `SCL -> GPIO9`
    - `VCC -> 3.3V`
    - `GND -> GND`
  - MPU6050：
    - `SDA -> GPIO8`
    - `SCL -> GPIO9`
    - `VCC -> 3.3V`
    - `GND -> GND`
    - `AD0 / INT / XDA / XCL` 当前都不接
  - MAX98357A：
    - `BCLK -> GPIO4`
    - `LRC/LRCLK -> GPIO5`
    - `DIN -> GPIO6`
    - `VIN -> 5V`
    - `GND -> GND`
  - Speaker：
    - `SPK+ / SPK-` 直接接 3W 喇叭两根线
- 当前实测结果：
  - MPU6050 地址为 `0x68`
  - 静止时合加速度 `M ≈ 1.02 ~ 1.04g`
  - 模块晃动时加速度/角速度变化正常
  - OLED 可正常显示实时数据
  - 跌倒测试已能触发喇叭报警

## 6. 跌倒状态机（当前测试版）
- 状态：
  - `NORMAL`
  - `FREEFALL`
  - `IMPACT`
  - `ALERT`
  - `MONITOR`
- 当前判定逻辑：
  - `FREEFALL`：`M < 0.6g`
  - `IMPACT`：在短时间窗口内出现：
    - `M >= 1.8g`
    - `Gmax >= 60 dps`
  - `ALERT`：撞击后出现“落地后相对静止”
  - `MONITOR`：连续报警结束后转为间歇提醒
- 当前用途定位：
  - 这是“桌面演示友好版”阈值
  - 优先保证可测试、可复现
  - 不是最终医学/产品级跌倒算法

## 7. 喇叭报警策略（当前测试版）
- 独立喇叭测试环境：
  - `max98357-speaker-test`
  - 入口：`src/max98357_speaker_test.cpp`
- 当前 `mpu6050-oled-test` 中已融合报警音
- 报警策略：
  - 先连续报警约 `12 秒`
  - 再间歇报警约 `30 秒`
  - 如果检测到明显恢复活动，可自动取消报警
- 报警音型：
  - `880Hz`
  - `1320Hz`
  - 双音交替，适合演示

## 8. 当前经验与注意事项
- `MPU6050` 和 `MAX98357A` 已经分别单独验证通过，也已在同一测试固件中联合验证通过。
- `MAX98357A` 供电优先使用 `5V`，不要只给 `3.3V` 后就直接判断故障。
- 普通 3W 喇叭不能直接接电源，必须通过 `MAX98357A` 这种功放模块驱动。
- 当前跌倒阈值已经针对“手持洞洞板、小高度自由下落演示”适度放宽。

## 9. 常用命令
- MLX90614 测试：
  - `python -m platformio run -e mlx90614-oled-test -t upload`
  - `python -m platformio device monitor -e mlx90614-oled-test -p COM4`
- MPU6050 + 跌倒测试：
  - `python -m platformio run -e mpu6050-oled-test -t upload`
  - `python -m platformio device monitor -e mpu6050-oled-test -p COM4`
- MAX98357A 喇叭测试：
  - `python -m platformio run -e max98357-speaker-test -t upload`
  - `python -m platformio device monitor -e max98357-speaker-test -p COM4`

## 10. 后续建议
- 如果继续推进主线：
  - 先把 `mpu6050-oled-test` 的状态机实测稳定
  - 再考虑并回主程序
- 如果要进一步提升产品感：
  - 增加“误报取消/确认”输入（按钮或触摸）
  - 增加联网通知或语音播报

## 11. 2026-03-08 主程序回并记录（MPU6050 + Alarm Merged to Main）
- 本次回并目标：
  - 将已经在独立测试环境验证通过的 `MPU6050` 跌倒检测与 `MAX98357A` 本地报警逻辑合并进主程序。
  - 不改动既有 `MAX30102` 心率/血氧主链路的整体结构。
- 已回并到主程序的能力：
  - `MPU6050` 初始化与周期采样
  - 跌倒状态机：`NORMAL / FREEFALL / IMPACT / ALERT / MONITOR`
  - `MAX98357A` I2S 报警音输出
  - OLED 增加跌倒状态显示
  - 串口增加 `Fall / M / Gmax / MPU / SPK` 字段
- 当前主程序采用的跌倒阈值：
  - `FREEFALL`：`M < 0.6g`
  - `IMPACT`：`M > 1.8g` 且 `Gmax > 60 dps`
  - `STILL`：`M` 回到 `0.85~1.15g` 且 `Gmax < 25 dps`
  - 连续报警约 `12s`
  - 间歇提醒约 `30s`
- 报警策略：
  - 无按钮条件下，不采用“无限响直到人为关闭”。
  - 改为“先连续报警，再间歇提醒，超时自动结束”。
- 工程配置调整：
  - `platformio.ini` 主环境增加 `build_src_filter`
  - 原因：避免 `mlx90614_oled_test.cpp / mpu6050_oled_test.cpp / max98357_speaker_test.cpp`
    在主环境编译时与 `main.cpp` 的 `setup()/loop()` 冲突。
- 编译状态：
  - `python -m platformio run -e esp32-s3-devkitc-1`
  - 结果：编译通过
- 当前主线状态：
  - 独立 test 环境继续保留，用于后续单模块回归验证。
  - 主程序现在已具备：
    - 心率/血氧显示
    - 温度显示（当前默认 `MAX30102 die temperature`）
    - 跌倒检测
    - 本地声音报警

## 12. 2026-03-10 心率血氧参数回退记录（Fast Realtime Profile Restored）
- 背景：
  - 后续强化平滑版本在部分实测中出现 `SpO2` 长时间 `--`，且 `HR` 观感不稳定。
- 当前回退到的测试友好参数：
  - `kCalibrationDurationMs = 3000`
  - `kAlgoStepSamples = 25`
  - `kCalibrationMinWindows = 1`
  - 保留 `algoHeartRate` 回退通道（减少 `HR=--`）
  - 保持 `kSpo2DisplayHoldMs = 15000`、`kInvalidStreakDrop = 8` 的此前配置
- 本次回退的目的：
  - 优先恢复“更快出值 + 可演示”的体验，再做小步参数微调。

## 13. 2026-03-10 HR 连续显示与异常值抑制实验（Based on Real Logs）
- 现象：
  - 用户实测日志显示：`HR` 在 `POOR` 质量下易长时间 `--`，且偶发偏高尖峰。
- 本次在主程序的调参项：
  - `kFingerLostDebounceMs: 1200 -> 1800`
  - `kHrDisplayHoldMs: 8000 -> 15000`
  - `kMaxAcceptedBpm: 140 -> 125`
  - `kMinAcceptedSpo2: 85 -> 90`
  - `kMaxHrJumpPerUpdate: 20 -> 16`
  - 新增 `kHrValidStreakRequired = 2`（HR 更稳）
  - 新增 `kSpo2ValidStreakRequired = 1`（SpO2 保持出值）
  - 新增算法回退门槛：
    - `kAlgoHrMinQualityRatio = 0.28`
    - `kAlgoHrMaxDeltaFromDisplay = 15`
  - 新增 `lastHrDisplayRefreshMs` 作为 HR 显示保持时间基准
- 当前结论：
  - 该版属于“低风险小步优化”，优先改显示连续性与异常尖峰，未引入大算法重构。
