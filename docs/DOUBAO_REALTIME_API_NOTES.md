# Doubao Realtime API 接入记录（S2S）

更新时间：2026-03-13

## 1. 文档目的
- 固化当前已确认的豆包端到端实时语音模型（Realtime API）接入信息。
- 作为后续“语音问答优化”阶段的基线配置文档。

## 2. 当前确认的接口
- 接口类型：豆包端到端实时语音大模型 API（S2S, Speech-to-Speech）
- 协议：WebSocket（二进制协议）
- WS URL：`wss://openspeech.bytedance.com/api/v3/realtime/dialogue`
- 参考文档（当前确认）：`https://www.volcengine.com/docs/6561/1594356?lang=zh`

## 3. 必要请求头参数
在建立 WebSocket 连接时，需要携带以下 Header：

1. `X-Api-App-ID`
- 你的语音应用 AppID（控制台获取）

2. `X-Api-Access-Key`
- 你的 Access Token / Access Key（控制台获取）

3. `X-Api-Resource-Id`
- 固定值：`volc.speech.dialog`

4. `X-Api-App-Key`
- 固定值：`PlgvMymc7f3tQnJ6`

5. `X-Api-Connect-Id`（可选但建议）
- 客户端生成的连接跟踪 ID（便于排障）

## 4. StartSession 必传与推荐参数
`StartSession` 事件中，当前至少需要关注：

1. `model`（必传）
- 可选：`O`、`SC`、`1.2.1.0`、`2.2.0.0`
- 推荐默认：`1.2.1.0`（O2.0）

2. TTS `speaker`（建议显式指定）
- 推荐默认：`zh_female_vv_jupiter_bigtts`

3. 音频格式建议
- 上行音频：`PCM`，`16000Hz`，`单声道`，`int16`，小端序
- 下行音频：优先 `pcm_s16le`，`24000Hz`，单声道

## 5. 最小会话流程（建议）
1. `StartConnection`
2. `StartSession`（带 model + tts 配置）
3. 循环发送 `TaskRequest`（音频包，建议 20ms/包）
4. 接收事件：
- `ASRInfo` / `ASRResponse` / `ASREnded`
- `ChatResponse` / `ChatEnded`
- `TTSResponse` / `TTSEnded`
5. 完成后发送 `FinishSession`
6. 可选：`FinishConnection`

## 6. 当前项目落地建议
1. 先跑通“最小闭环”
- 语音输入 -> 模型理解 -> 语音输出

2. 再做工程增强
- 中断打断（barge-in）
- 上下文管理（ConversationCreate/Update/Retrieve）
- 联网搜索 / 外部 RAG
- 音色切换与人设优化

3. 与当前 ESP32 项目协同方式（建议）
- ESP32：采集麦克风、播放返回音频、显示会话状态
- 中间服务（PC/边缘服务器）：负责 WS 协议编解码与会话管理

## 7. 待补充（后续优化时填写）
- [ ] 控制台参数截图位置
- [ ] 实际使用的 `AppID`
- [ ] 实际使用的 `Access Key`（不要写入仓库，可写“已配置到本地环境变量”）
- [ ] 最终确定的 `model` 与 `speaker`
- [ ] 单轮平均时延与稳定性测试记录
