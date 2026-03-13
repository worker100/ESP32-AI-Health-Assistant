from __future__ import annotations

import base64
import asyncio
import json
from typing import Any

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse

from config import settings
from doubao_client import DoubaoRealtimeBridge
from models import BackendMessage, DeviceMessage

app = FastAPI(title="ESP32 AI Health Assistant Backend")
bridge = DoubaoRealtimeBridge()


@app.get("/health")
async def health() -> JSONResponse:
    return JSONResponse(
        {
            "ok": True,
            "device_ws_path": settings.device_ws_path,
            "doubao_credentials_ready": bridge.credentials_ready(),
            "doubao_ws_url": settings.doubao_ws_url,
            "doubao_model": settings.doubao_model,
        }
    )


async def send_backend_message(websocket: WebSocket, message: BackendMessage) -> None:
    await websocket.send_text(message.model_dump_json())


def parse_device_message(raw: str) -> DeviceMessage:
    data: Any = json.loads(raw)
    return DeviceMessage.model_validate(data)


def chunk_bytes(data: bytes, chunk_size: int) -> list[bytes]:
    return [data[i : i + chunk_size] for i in range(0, len(data), chunk_size)]


async def forward_audio_session(
    websocket: WebSocket,
    session_id: str,
    live_bridge: DoubaoRealtimeBridge,
) -> None:
    reply_text = ""
    try:
        while True:
            packet = await live_bridge.recv_packet()

            if packet.event == 451 and packet.payload_json:
                results = packet.payload_json.get("results", [])
                if results:
                    text = results[-1].get("text", "")
                    is_interim = bool(results[-1].get("is_interim", False))
                    await send_backend_message(
                        websocket,
                        BackendMessage(
                            type="asr_partial" if is_interim else "asr_final",
                            session_id=session_id,
                            text=text,
                        ),
                    )

            elif packet.event == 550 and packet.payload_json:
                content = packet.payload_json.get("content", "")
                reply_text += content
                await send_backend_message(
                    websocket,
                    BackendMessage(
                        type="reply_text",
                        session_id=session_id,
                        text=content,
                    ),
                )

            elif packet.event == 352:
                for chunk in chunk_bytes(packet.payload, 1024):
                    await send_backend_message(
                        websocket,
                        BackendMessage(
                            type="tts_chunk",
                            session_id=session_id,
                            payload_b64=base64.b64encode(chunk).decode("ascii"),
                            sample_rate=24000,
                        ),
                    )

            elif packet.event == 359:
                await send_backend_message(
                    websocket,
                    BackendMessage(
                        type="tts_end",
                        session_id=session_id,
                        detail="live_round_tts_done",
                        text=reply_text,
                    ),
                )
                break

            elif packet.event in {153, 599}:
                raise RuntimeError(str(packet.payload_json))
    finally:
        await live_bridge.finish_session()
        await live_bridge.close()


@app.websocket("/ws/device")
async def device_socket(websocket: WebSocket) -> None:
    await websocket.accept()
    await send_backend_message(
        websocket,
        BackendMessage(
            type="backend_status",
            detail="connected",
        ),
    )
    live_bridge: DoubaoRealtimeBridge | None = None
    forward_task: asyncio.Task[None] | None = None

    try:
        while True:
            raw = await websocket.receive_text()
            message = parse_device_message(raw)

            if message.type == "hello":
                await send_backend_message(
                    websocket,
                    BackendMessage(
                        type="backend_status",
                        session_id=message.session_id,
                        detail="hello_ack",
                    ),
                )
                continue

            if message.type == "start_session":
                if not bridge.credentials_ready():
                    await send_backend_message(
                        websocket,
                        BackendMessage(
                            type="error",
                            session_id=message.session_id,
                            detail=(
                                "Doubao credentials not configured in backend/.env"
                            ),
                        ),
                    )
                    continue

                if message.text:
                    await send_backend_message(
                        websocket,
                        BackendMessage(
                            type="session_started",
                            session_id=message.session_id,
                            detail="text_round_start",
                        ),
                    )
                    await bridge.connect()
                    try:
                        await bridge.start_text_session()
                        result = await bridge.collect_text_round(query=message.text)
                        await send_backend_message(
                            websocket,
                            BackendMessage(
                                type="reply_text",
                                session_id=message.session_id,
                                text=result["reply_text"],
                                detail="text_round_reply",
                            ),
                        )

                        if result["audio_bytes"] > 0 and result.get("audio_data"):
                            for chunk in chunk_bytes(result["audio_data"], 2048):
                                await send_backend_message(
                                    websocket,
                                    BackendMessage(
                                        type="tts_chunk",
                                        session_id=message.session_id,
                                        payload_b64=base64.b64encode(chunk).decode("ascii"),
                                        sample_rate=24000,
                                    ),
                                )
                            await send_backend_message(
                                websocket,
                                BackendMessage(
                                    type="tts_end",
                                    session_id=message.session_id,
                                    detail="text_round_tts_done",
                                ),
                            )
                    finally:
                        await bridge.finish_session()
                        await bridge.close()
                    continue

                live_bridge = DoubaoRealtimeBridge()
                await live_bridge.connect()
                await live_bridge.start_audio_session()
                await send_backend_message(
                    websocket,
                    BackendMessage(
                        type="session_started",
                        session_id=message.session_id,
                        detail="live_audio_round_start",
                    ),
                )
                forward_task = asyncio.create_task(
                    forward_audio_session(websocket, message.session_id or "", live_bridge)
                )
                continue

            if message.type == "audio_chunk":
                if live_bridge is not None and message.payload_b64:
                    audio_bytes = base64.b64decode(message.payload_b64)
                    await live_bridge.send_audio_chunk(audio_bytes)
                    continue
                await send_backend_message(
                    websocket,
                    BackendMessage(
                        type="backend_status",
                        session_id=message.session_id,
                        detail="audio_chunk_received",
                    ),
                )
                continue

            if message.type == "interrupt":
                await send_backend_message(
                    websocket,
                    BackendMessage(
                        type="backend_status",
                        session_id=message.session_id,
                        detail="interrupt_ack",
                    ),
                )
                continue

            if message.type == "stop_session":
                if forward_task is not None:
                    await asyncio.wait_for(forward_task, timeout=15.0)
                    forward_task = None
                    live_bridge = None
                await send_backend_message(
                    websocket,
                    BackendMessage(
                        type="backend_status",
                        session_id=message.session_id,
                        detail="session_stop_ack",
                    ),
                )
                continue

            if message.type == "device_status":
                await send_backend_message(
                    websocket,
                    BackendMessage(
                        type="backend_status",
                        session_id=message.session_id,
                        detail=f"device_state={message.state or 'unknown'}",
                    ),
                )
                continue

    except WebSocketDisconnect:
        if forward_task is not None:
            forward_task.cancel()
        return
