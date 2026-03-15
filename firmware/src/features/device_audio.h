#pragma once

#include <stdint.h>

// 设备与音频层（device/audio）：
// - 硬件初始化（I2C, OLED, MAX30102, MPU6050, MLX90614, I2S）
// - Speaker/Mic 底层控制
// - VAD 与本地报警音播放

// 硬件初始化序列（init sequence）。
void initI2c();
void i2cScan();
void initOled();
void initMax30102();
void initMpu6050();
void initI2sAudio();
void initMicI2s();

// Speaker 控制辅助函数。
void ensureSpeakerActive();
void ensureSpeakerMuted();
void ensureSpeakerSampleRate(uint32_t sampleRate);

// 语音/报警运行任务。
void updateVoiceVad(uint32_t now);
void playAlarmBurst(bool monitorPhase);
void playPromptTone();

// 温度与 MPU 采样任务。
void initMlx90614(bool verbose = true);
void updateTemperature();
void readMpuSample();
