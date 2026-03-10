# ESP32-AI-Health-Assistant

## 目录结构
- `docs/PROJECT_MEMORY.md`：项目永久记忆（关键事实、阶段目标、引脚与接口）
- `firmware/`：PlatformIO + Arduino 固件工程
- `hardware/`：硬件资料索引

## 快速开始（Trae）
1. 打开 `firmware` 目录。
2. 安装并启用 `PlatformIO IDE` 扩展。
3. 构建：`Build`。
4. 下载：`Upload`。
5. 串口：`Monitor`（115200）。

## 当前最小系统
- ESP32-S3 + MAX30102 + SSD1306 OLED
- I2C：SDA=GPIO8，SCL=GPIO9
- 地址：OLED=0x3C，MAX30102=0x57
