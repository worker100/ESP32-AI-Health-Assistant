#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// 主程序正式配置文件
// 说明：
// 1. 这里放的是主程序和实时语音链路都会用到的正式配置。
// 2. 修改这里的参数后，需要重新编译并烧录固件。
// 3. 如果只是改文本测试那一句提问内容，再去改 voice_backend_test_config.h。
// ---------------------------------------------------------------------------

// 网络 / 后端配置 -----------------------------------------------------------
// Wi-Fi 名称
constexpr char kProjectWifiSsid[] = "888";
// Wi-Fi 密码
constexpr char kProjectWifiPassword[] = "888ye999";
// 本地后端电脑的局域网 IP
constexpr char kProjectBackendHost[] = "192.168.1.67";
// 本地后端端口
constexpr uint16_t kProjectBackendPort = 8787;
// WebSocket 路径
constexpr char kProjectBackendPath[] = "/ws/device";
// 是否默认开启主程序到后端的桥接
constexpr bool kProjectBackendBridgeDefaultEnabled = true;

// 喇叭音量预设 --------------------------------------------------------------
// 三档可选：
// 1. SpeakerVolumePreset::Low    -> 小声
// 2. SpeakerVolumePreset::Medium -> 中等
// 3. SpeakerVolumePreset::High   -> 大声
// 你平时只需要改下面这个 kSpeakerVolumePreset 即可。
enum class SpeakerVolumePreset : uint8_t {
  Low = 0,
  Medium = 1,
  High = 2,
};

// 三档音量对应的放大倍数
// 说明：
// 1. Low   = 明显压低音量，适合近距离测试
// 2. Medium = 接近原始音量，适合作为常规默认值
// 3. High  = 明显放大，适合环境较吵时测试
// 注意：
// 这里只是数字音量增益，修改后必须重新编译并烧录固件，开发板上的声音才会变化。
constexpr float kSpeakerGainLow = 0.35f;
constexpr float kSpeakerGainMedium = 0.75f;
constexpr float kSpeakerGainHigh = 1.10f;

// 当前默认音量档位：
// 改成 Low / Medium / High 任意一个，然后重新编译烧录即可生效。
// 如果你觉得还是太大声，优先先把这里改成 Low，再重新烧录测试。
constexpr SpeakerVolumePreset kSpeakerVolumePreset = SpeakerVolumePreset::Low;

constexpr float speakerGainFromPreset(SpeakerVolumePreset preset) {
  return (preset == SpeakerVolumePreset::Low)
             ? kSpeakerGainLow
             : ((preset == SpeakerVolumePreset::High) ? kSpeakerGainHigh : kSpeakerGainMedium);
}

// 实际用于后端 TTS 播放的音量增益，一般不用手改，跟随上面的档位自动变化。
constexpr float kBackendTtsGain = speakerGainFromPreset(kSpeakerVolumePreset);

// 语音 / 后端常用阈值 -------------------------------------------------------
// 后端状态上报间隔
constexpr uint32_t kBackendStatusPushMs = 1500;
// 后端断线后重试间隔
constexpr uint32_t kBackendRetryMs = 5000;
// Wi-Fi 连接超时时间
constexpr uint32_t kBackendWifiConnectTimeoutMs = 20000;
// 播报时插话打断阈值因子
constexpr float kBackendInterruptOnFactor = 3.4f;
// 播报时插话最小 RMS 门槛
constexpr float kBackendInterruptMinRms = 0.015f;
// 连续多少帧满足条件才触发插话打断
constexpr uint8_t kBackendInterruptTriggerFrames = 2;
// 说话结束后，尾部继续补发音频的保留时间
constexpr uint32_t kBackendTailStreamMs = 350;

// 麦克风 VAD 校准时间
constexpr uint32_t kMicVadCalMs = 3000;
// 噪声底估计平滑系数
constexpr float kMicVadNoiseAlpha = 0.08f;
// 进入“说话中”的触发倍数
constexpr float kMicVadOnFactor = 2.8f;
// 退出“说话中”的触发倍数
constexpr float kMicVadOffFactor = 1.8f;
// 进入“说话中”的最小绝对门槛
constexpr float kMicVadMinOn = 0.012f;
// 退出“说话中”的最小绝对门槛
constexpr float kMicVadMinOff = 0.0065f;
// 说话结束保持时间，避免太快收口
constexpr uint32_t kMicVadOffHoldMs = 450;
// 两次唤醒之间的最小间隔
constexpr uint32_t kMicVadMinTriggerIntervalMs = 1200;
// 连续几帧满足条件才判定开始说话
constexpr uint8_t kMicVadConsecutiveOnFrames = 2;
// 连续几帧满足条件才判定说话结束
constexpr uint8_t kMicVadConsecutiveOffFrames = 2;
// 一轮监听窗口时间
constexpr uint32_t kVoiceListenWindowMs = 4500;

// 健康数据显示 / 平滑阈值 --------------------------------------------------
// 心率显示保持时间，超过后可回退为 --
constexpr uint32_t kHrDisplayHoldMs = 15000;
// 血氧显示保持时间，超过后可回退为 --
constexpr uint32_t kSpo2DisplayHoldMs = 25000;
// 接受的最小 / 最大心率
constexpr float kMinAcceptedBpm = 45.0f;
constexpr float kMaxAcceptedBpm = 125.0f;
// 接受的最小 / 最大血氧
constexpr int32_t kMinAcceptedSpo2 = 82;
constexpr int32_t kMaxAcceptedSpo2 = 100;
