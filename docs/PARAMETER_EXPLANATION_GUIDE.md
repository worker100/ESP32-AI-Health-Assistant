# 参数解释手册（给你看）

更新时间：2026-03-11  
适用文件：`firmware/src/main.cpp`（当前主程序）

## 1. 文档用途
- 这份文档只做“读参数、看串口、理解判定”的说明。
- 不作为永久记忆日志，不记录开发过程，只讲“这个参数是什么意思、正常值大概是多少”。

## 2. 串口字段速查
- `HR`：心率（BPM）
- `O2`：血氧饱和度（%）
- `T`：温度（当前默认是 `MLX90614 object temp`）
- `F`：跌倒状态（`NORMAL/FREEFALL/IMPACT/ALERT/MONITOR`）
- `Thr(FF<... IMP>... G>...)`：当前跌倒关键阈值摘要
- `Q`：信号质量（`NO FINGER/WAITING/CALIBRATING/POOR/FAIR/GOOD`）
- `C`：内部置信度（0~100，辅助调参观察）
- `S`：测量流程状态（`WAIT/CAL/RUN`）
- `LED`：MAX30102 当前 LED 幅度（十六进制）
- `Src`：温度来源（`MLX/MAX/NONE`）

## 3. 心率（HR）参数说明
当前关键参数：
- `kMinAcceptedBpm = 45`
- `kMaxAcceptedBpm = 125`
- `kHrEwmaAlpha = 0.30`
- `kHrDisplayHoldMs = 15000`
- `kBeatTimeoutMs = 12000`

含义：
- 只有 `45~125 BPM` 的候选值会被接受（用于演示稳定，过滤离谱值）。
- `EWMA` 用于平滑，数值不会瞬时跳很大。
- 如果短时间没有新心跳，显示会先“保留”；超过保持时间会掉 `--`。

正常参考范围（成人静息）：
- 常见：`60~100 BPM`
- 运动/紧张时可能更高；睡眠/体质差异可能更低。

## 4. 血氧（O2）参数说明
当前关键参数：
- `kMinAcceptedSpo2 = 82`
- `kMaxAcceptedSpo2 = 100`
- `kSpo2EwmaAlpha = 0.25`
- `kSpo2DisplayHoldMs = 25000`
- `kInvalidStreakDrop = 28`

SpO2 当前实现是“双通道候选”：
1. 主通道：Maxim 原算法输出
2. 回退通道：窗口 `AC/DC ratio` 估算（当主通道失效时兜底）

含义：
- 放宽了下限和保持时间，目标是减少 `O2=--%` 长时间无值。
- `InvalidStreak` 增大后，不会轻易掉 `--%`，更偏演示连续性。

正常参考范围（常见）：
- 常见：`95%~100%`
- 持续低于 `92%` 通常需要重点关注（仅做常识参考，非医疗诊断）。

## 5. 温度（T）参数说明
当前读取：
- `MLX90614` 每 `1000ms` 读取一次（`kTempReadMs = 1000`）
- MLX 不可用时，回退到 `MAX30102 die temp`（每 `2000ms`）

含义：
- 你现在串口 `T` 显示的是红外目标温度（`object temp`），不是医学体温结论。
- 如果传感器没有稳定对准人体表面，`T` 常会接近环境温度（比如 23~27℃）。

## 6. 跌倒检测参数（重点）
当前关键阈值：
- `kFreeFallThresholdG = 0.80`
- `kImpactThresholdG = 0.95`
- `kImpactRotationThresholdDps = 20`
- `kImpactWindowMs = 1300`
- `kStillWindowMs = 700`
- `kStillAccelMinG = 0.85`
- `kStillAccelMaxG = 1.15`
- `kStillGyroThresholdDps = 40`
- `kRecoveryAccelThresholdG = 1.55`
- `kRecoveryGyroThresholdDps = 110`

### 6.1 每个状态是什么意思
- `NORMAL`：普通状态
- `FREEFALL`：检测到失重阶段（接近“滞空”）
- `IMPACT`：检测到碰撞冲击
- `ALERT`：满足“跌倒并落地后静止”，进入连续报警
- `MONITOR`：连续报警后进入间歇提醒

### 6.2 跌倒触发链路（必须按顺序）
1. `NORMAL -> FREEFALL`：`M < 0.80g`
2. 在 `1300ms` 内 `FREEFALL -> IMPACT`：`M > 0.95g` 且 `Gmax > 20dps`
3. `IMPACT` 后在 `700ms` 观察到相对静止（`M` 在 `0.85~1.15` 且 `Gmax < 40`）进入 `ALERT`

说明：
- 只出现 `FREEFALL` 但没到 `IMPACT/ALERT` 时，不会响。
- 这套阈值是“桌面小高度演示友好版”，不是产品级最终算法。

## 7. M / Gmax / A 怎么看
- `M`：合加速度（`sqrt(ax^2 + ay^2 + az^2)`，单位 g）
  - 静止常见约 `1.0g`
  - 失重阶段会明显掉到 `1g` 以下
- `Gmax`：三轴角速度绝对值最大值（单位 dps）
  - 静止时通常较小
  - 碰撞/甩动时会上升
- `A=ax,ay,az`：三轴加速度分量（单位 g）
  - 用来观察姿态变化和冲击方向

## 8. 你做测试时的快速判断
- 如果日志出现 `F=FREEFALL -> IMPACT -> ALERT`，报警链路就打通了。
- 如果总是 `F=FREEFALL -> NORMAL`，说明“撞击条件”没满足（常见是 `M` 或 `Gmax` 不够）。
- 如果一直 `F=NORMAL`，说明连失重都没进（抬起/自由落体动作不明显）。

## 9. 重要提醒
- 这些阈值是演示调试参数，不是医疗设备参数。
- 心率/血氧数值会受佩戴、接触压力、遮光、手部状态影响很大。
