# Voice Assistant Edge-Cloud Plan

Updated: 2026-03-13

## 1. Goal
- Build a demo-ready AI voice assistant on top of the current ESP32-S3 health device.
- Keep ESP32 as the edge device for audio capture, playback, display, and local state control.
- Offload speech understanding and reply generation to a local backend service running on the user's computer.
- Let the backend proxy audio/session traffic to Volcengine Doubao Realtime API.

## 2. Why This Architecture
- ESP32-S3 is suitable for real-time audio I/O and device control, but not for running a full speech+LLM+TTS model locally.
- Doubao Realtime API uses WebSocket + binary event protocol; implementing and debugging this directly on ESP32 would add substantial complexity.
- Keeping API keys and session orchestration on the backend avoids exposing cloud credentials in device firmware.
- This design still supports a "no laptop in final demo" path later by moving the same backend from PC to a cloud host or NAS.

## 3. Current Hardware Baseline
- Main board: ESP32-S3
- Mic: INMP441
  - `BCLK=GPIO15`
  - `WS=GPIO16`
  - `DATA=GPIO17`
- Speaker amp: MAX98357A
  - `BCLK=GPIO4`
  - `LRC=GPIO5`
  - `DIN=GPIO6`
- Sensors:
  - MAX30102
  - MLX90614
  - MPU6050
- Display:
  - SSD1306 OLED

## 4. System Split

### ESP32 responsibilities
- Capture microphone PCM frames from INMP441.
- Run lightweight local VAD / wake gating.
- Maintain local voice state machine:
  - `IDLE`
  - `LISTENING`
  - `STREAMING_UP`
  - `THINKING`
  - `PLAYING`
  - `INTERRUPTED`
  - `ERROR`
- Send audio packets to backend over Wi-Fi.
- Receive reply audio stream from backend and play via MAX98357A.
- Show current status on OLED and serial logs.
- Keep health sensing and fall detection as first-class local functions.

### Local backend responsibilities
- Hold Doubao credentials and establish Realtime API session.
- Translate simple app-level messages from ESP32 into Doubao session/audio events.
- Receive:
  - audio frames from ESP32
  - optional device metadata/events
- Forward audio to Doubao Realtime API.
- Receive from Doubao:
  - ASR events
  - chat events
  - TTS audio events
- Stream TTS audio back to ESP32.
- Log transcripts and timing for debugging.

## 5. Recommended Transport Between ESP32 and Backend
- Protocol for phase 1: WebSocket
- Reason:
  - full duplex
  - easy state/event streaming
  - simple backend implementation in Python or Node.js

### ESP32 -> Backend event types
- `hello`
- `start_session`
- `audio_chunk`
- `stop_session`
- `interrupt`
- `ping`
- `device_status`

### Backend -> ESP32 event types
- `session_started`
- `asr_partial`
- `asr_final`
- `reply_text`
- `tts_chunk`
- `tts_end`
- `error`
- `backend_status`

## 6. Audio Format Baseline

### Upload path
- Source: INMP441
- Format: PCM `s16le`
- Sample rate: `16000`
- Channels: `1`
- Packet size target: `20ms`
- Payload size target per packet: `640 bytes`

### Playback path
- Backend receives TTS audio from Doubao.
- Preferred return format to ESP32: PCM `s16le`
- Sample rate: `24000`
- Channels: `1`
- ESP32 playback path converts/feeds directly into I2S TX for MAX98357A.

## 7. Doubao Realtime Session Strategy
- Use Doubao Realtime API through backend only.
- Initial defaults:
  - `model = 1.2.1.0`
  - `speaker = zh_female_vv_jupiter_bigtts`
  - `X-Api-Resource-Id = volc.speech.dialog`
  - `X-Api-App-Key = PlgvMymc7f3tQnJ6`

### Session lifecycle
1. Backend opens WebSocket to Doubao.
2. Backend sends `StartConnection`.
3. When ESP32 enters conversation, backend sends `StartSession`.
4. ESP32 streams `audio_chunk`.
5. Backend forwards Doubao `TaskRequest` audio frames.
6. Backend receives:
  - `ASRResponse`
  - `ChatResponse`
  - `TTSResponse`
7. Backend forwards text/audio stream back to ESP32.
8. Backend sends `FinishSession` after round completion or timeout.

## 8. OLED / Serial UX Proposal

### OLED voice states
- `VOICE: IDLE`
- `VOICE: LISTEN`
- `VOICE: THINK`
- `VOICE: SPEAK`
- `VOICE: ERR`

### Serial additions
- `VA=IDLE/LISTEN/THINK/SPEAK/ERR`
- `ASR=...` final transcript line
- `AI=...` short reply text line
- `LAT=up/asr/llm/tts/total`

## 9. Phase Plan

### Phase 1: Backend-only verification
- Build local backend service.
- Manually feed a PCM file to Doubao.
- Verify:
  - session starts
  - ASR text returns
  - TTS audio returns
  - timing is acceptable

### Phase 2: ESP32 uplink only
- ESP32 captures live mic audio.
- Send audio frames to backend.
- Backend prints ASR transcript.
- No playback yet.

### Phase 3: Full duplex demo
- Backend streams TTS audio to ESP32.
- ESP32 plays audio through MAX98357A.
- OLED and serial show live assistant state.

### Phase 4: Interaction polish
- Add interrupt/barge-in.
- Add short reply constraint for lower latency.
- Add health-domain prompt.
- Optionally add external RAG or web search.

## 10. Recommended Backend Stack
- First implementation: Python
- Suggested libraries:
  - `fastapi`
  - `uvicorn`
  - `websockets`
  - optional `pydantic`
- Reason:
  - fast iteration
  - easier binary/event debugging
  - enough for single-device demo

## 11. Latency Budget Target
- VAD / local gate: `100-300 ms`
- ESP32 -> backend uplink: `50-150 ms`
- Backend -> Doubao -> first text/audio response: `500-1500 ms`
- Playback start: `200-600 ms`
- Target first audible reply: `~1.0s to 2.5s`

## 12. Risks and Controls
- Risk: Wi-Fi instability
  - Control: reconnect logic, short buffering, explicit error state
- Risk: overlong LLM response causes high latency
  - Control: system prompt requesting concise answers
- Risk: playback echo retriggers VAD
  - Control: playback-time mic gate and interrupt threshold tuning
- Risk: API key exposure
  - Control: keep credentials only in backend env vars

## 13. Immediate Next Coding Steps
1. Create a `backend/` service in the repo with a local WebSocket server.
2. Implement a Doubao session client in the backend.
3. Add a test mode that sends a local PCM file and logs ASR/TTS events.
4. Extend ESP32 firmware with a backend client state machine stub.
5. Add serial/OLED voice state fields before full audio streaming is merged.
