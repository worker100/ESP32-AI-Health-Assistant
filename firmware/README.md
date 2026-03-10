# Firmware (PlatformIO + Arduino)

## 1. 推荐方式（Trae）
1. 在 Trae 安装 `PlatformIO IDE` 扩展。
2. 打开本目录 `firmware`。
3. 选择环境 `esp32-s3-devkitc-1`。
4. 执行 `Build` / `Upload` / `Monitor`。

测试环境与独立验证记录见：`TEST_MEMORY.md`

## 2. 串口参数
- Baud rate: `115200`

## 3. 当前最小系统
- I2C: `SDA=GPIO8`, `SCL=GPIO9`
- OLED(SSD1306): `0x3C`
- MAX30102: `0x57`

## 4. 说明
- 当前阶段先稳定心率显示。
- 串口中的 `SpO2(est)` 为 bring-up 期估算值，仅用于联调参考，不作为医学/算法结论。
- 后续建议接入标准 SpO2 算法后再用于正式展示。
