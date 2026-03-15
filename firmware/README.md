# Firmware (PlatformIO + Arduino)

## 1. 目录结构
- 主固件入口：`src/main.cpp`
- 共享状态与类型：`src/core/`
- 驱动与硬件访问：`src/drivers/`
- 业务功能模块：`src/features/`
- 通用模块：`src/modules/`
- 独立测试程序：`test_apps/`

## 2. 环境用途
- `esp32-s3-devkitc-1`：主固件
- `mlx90614-oled-test`：MLX90614 + OLED
- `mlx90614-quick-test`：MLX90614 快速连通
- `mpu6050-oled-test`：MPU6050 + OLED
- `max98357-speaker-test`：MAX98357A 喇叭
- `inmp441-i2s-test`：INMP441 采音
- `voice-backend-text-test`：后端文字链路
- `voice-backend-live-test`：后端实时语音链路

## 3. 常用命令
- 主固件编译：
```bash
python -m platformio run -e esp32-s3-devkitc-1
```

- 主固件下载（COM3）：
```bash
python -m platformio run -e esp32-s3-devkitc-1 -t upload --upload-port COM3
```

- 串口监视（COM3）：
```bash
python -m platformio device monitor -e esp32-s3-devkitc-1 -p COM3 -b 115200
```

- 查看串口列表：
```bash
python -m platformio device list
```

## 4. 运行时串口命令
- 跌倒模式：`MODE TEST` / `MODE NORMAL` / `MODE?`
- 后端桥接：`BACKEND ON` / `BACKEND OFF` / `BACKEND?`
- 音量：`VOL LOW` / `VOL MED` / `VOL HIGH` / `VOL?`
- 日志档位：`LOG DEMO` / `LOG FULL` / `LOG?`
- 健康状态：`HEALTH?`
- 帮助：`HELP`

## 5. 参数修改速查（重点）
- 配置总入口：`include/project_config.h`
- 默认参数文件：`include/config/project_defaults.h`
- 本地覆盖文件（推荐新建）：`include/config/project_local.h`
- 修改声音大小（持久生效）：
  - 默认档位：`AI_HEALTH_SPEAKER_PRESET`（0/1/2）
  - 三档增益：`AI_HEALTH_SPK_GAIN_LOW/MEDIUM/HIGH`
- 修改声音大小（临时调试）：
  - 串口命令：`VOL LOW` / `VOL MED` / `VOL HIGH`

## 6. 日志说明
- `LOG DEMO`：演示用简洁输出
- `LOG FULL`：调试用完整输出
- `BWS type=tts_end` 后会打印 `AI=...` 最终文本

## 7. 已知说明
- SparkFun MAX3010x 库会提示 `I2C_BUFFER_LENGTH` 宏重定义警告，当前不影响功能与编译。
- 测试代码已独立到 `test_apps/`，不会被主固件误编译。
