# Backend Prototype

This directory contains the local backend prototype for the ESP32 AI Health Assistant voice path.

## Goal
- Accept audio/control events from the ESP32 device over a simple WebSocket channel.
- Bridge those events to Volcengine Doubao Realtime API.
- Forward transcripts, reply text, and TTS audio back to the ESP32.

## Current Status
- Local WebSocket server scaffold is implemented.
- Device event schema is implemented.
- Doubao bridge class and config are scaffolded.
- Real Doubao binary protocol forwarding is not finished yet because runtime credentials are not configured.

## Run
1. Create a virtual environment if needed.
2. Install dependencies:

```bash
pip install -r requirements.txt
```

3. Copy `.env.example` to `.env` and fill in real credentials later.
4. Start the server:

```bash
uvicorn main:app --host 127.0.0.1 --port 8787 --reload
```

## Device Channel
- WebSocket path: `/ws/device`
- Expected message types from ESP32:
  - `hello`
  - `start_session`
  - `audio_chunk`
  - `stop_session`
  - `interrupt`
  - `device_status`

## Planned Next Step
- Implement the official Doubao Realtime WebSocket handshake and session lifecycle in `doubao_client.py`.

## Text-only Smoke Test
This is the fastest way to verify whether the Doubao credentials and realtime path are working before wiring ESP32 audio in.

```bash
python test_text_query.py
```

Expected result:
- Session starts successfully
- Reply text is printed in terminal
- Returned TTS audio is saved to `test_reply_audio.pcm`
