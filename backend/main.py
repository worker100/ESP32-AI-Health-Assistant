from __future__ import annotations

import base64
import asyncio
import json
import time
from typing import Any

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse

from config import settings
from doubao_client import DoubaoRealtimeBridge
from models import BackendMessage, DeviceMessage

app = FastAPI(title="ESP32 AI Health Assistant Backend")
bridge = DoubaoRealtimeBridge()
latest_device_context_by_device_id: dict[str, dict[str, Any]] = {}
latest_any_device_context: dict[str, Any] = {}
PRIMARY_HEALTH_DEVICE_ID = "esp32-ai-health-assistant-main"
CONTEXT_STALE_MS = 3000.0
# Keep last valid metrics for a short window when a single device_status frame
# temporarily omits numeric fields, so fresh sessions don't lose context.
METRIC_HOLD_MS = 15000.0
FRESH_STATUS_WAIT_MS = 1400.0
MAX_DEVICE_CONTEXTS = 32
context_update_events: dict[str, asyncio.Event] = {}
routed_text_round_sessions: set[str] = set()


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


def is_vitals_assessment_query(text: str) -> bool:
    q = text.strip().lower()
    if not q:
        return False

    metric_keywords = (
        "心率",
        "血氧",
        "体温",
        "指标",
        "数值",
        "hr",
        "spo2",
        "temperature",
    )
    judge_keywords = (
        "正常",
        "怎么样",
        "高不高",
        "低不低",
        "有没有问题",
        "是否正常",
        "ok",
        "normal",
    )

    direct_phrases = (
        "现在我这个数值正常吗",
        "我现在的数值正常吗",
        "这些数值正常吗",
        "请看一下我现在的数值是否正确",
        "数值是否正常",
    )

    if any(phrase in q for phrase in direct_phrases):
        return True
    has_metric = any(keyword in q for keyword in metric_keywords)
    has_judge = any(keyword in q for keyword in judge_keywords)
    return has_metric and has_judge


def build_vitals_assessment_query(user_query: str) -> str:
    return (
        f"{user_query}\n"
        "请严格结合当前设备上下文中的 heart_rate_bpm、spo2_percent、temperature_c 回答。"
        "先简要复述当前数值，再判断是否大致正常。"
        "如果 measurement_confidence 为 low/invalid 或 temperature_validity 非 body_screening，"
        "必须明确提示仅供参考并建议复测。"
        "不要说你看不到数值，除非上下文确实没有这些字段。"
    )


async def wait_for_fresh_device_context(
    *,
    device_id: str,
    updated_after_ms: float,
    timeout_ms: float = FRESH_STATUS_WAIT_MS,
) -> dict[str, Any]:
    current = latest_device_context_by_device_id.get(device_id, {})
    current_updated_at = current.get("_updated_at_ms")
    if isinstance(current_updated_at, (int, float)) and float(current_updated_at) >= updated_after_ms:
        return current.copy()

    event = context_update_events.setdefault(device_id, asyncio.Event())
    event.clear()
    deadline = time.monotonic() + (timeout_ms / 1000.0)
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0.0:
            break
        try:
            await asyncio.wait_for(event.wait(), timeout=remaining)
        except asyncio.TimeoutError:
            break
        event.clear()
        ctx = latest_device_context_by_device_id.get(device_id, {})
        updated_at = ctx.get("_updated_at_ms")
        if isinstance(updated_at, (int, float)) and float(updated_at) >= updated_after_ms:
            return ctx.copy()

    return latest_device_context_by_device_id.get(device_id, {}).copy()


async def run_text_round_and_stream(
    *,
    websocket: WebSocket,
    session_id: str,
    requester_device_id: str,
    local_context: dict[str, Any],
    query: str,
    tts_end_detail: str,
) -> None:
    round_bridge = DoubaoRealtimeBridge()
    await round_bridge.connect()
    try:
        await round_bridge.start_text_session(
            resolve_device_context(
                requester_device_id=requester_device_id,
                local_context=local_context,
            )
        )
        result = await round_bridge.collect_text_round(query=query)
        reply_text = result.get("reply_text", "")
        if reply_text:
            await send_backend_message(
                websocket,
                BackendMessage(
                    type="reply_text",
                    session_id=session_id,
                    text=reply_text,
                ),
            )

        if result.get("audio_bytes", 0) > 0 and result.get("audio_data"):
            for chunk in chunk_bytes(result["audio_data"], 2048):
                await send_backend_message(
                    websocket,
                    BackendMessage(
                        type="tts_chunk",
                        session_id=session_id,
                        payload_b64=base64.b64encode(chunk).decode("ascii"),
                        sample_rate=24000,
                    ),
                )
        await send_backend_message(
            websocket,
            BackendMessage(
                type="tts_end",
                session_id=session_id,
                detail=tts_end_detail,
                text=reply_text,
            ),
        )
    finally:
        await round_bridge.finish_session()
        await round_bridge.close()


def resolve_device_context(
    *,
    requester_device_id: str,
    local_context: dict[str, Any],
) -> dict[str, Any]:
    now_ms = time.time() * 1000.0
    primary_context = latest_device_context_by_device_id.get(PRIMARY_HEALTH_DEVICE_ID)
    requester_context = latest_device_context_by_device_id.get(requester_device_id)

    def age_ms(ctx: dict[str, Any] | None) -> float:
        if not ctx:
            return float("inf")
        updated_at_ms = ctx.get("_updated_at_ms")
        if not isinstance(updated_at_ms, (int, float)):
            return float("inf")
        return now_ms - float(updated_at_ms)

    primary_age = age_ms(primary_context)
    requester_age = age_ms(requester_context)

    if primary_context is not None:
        primary_is_fresh = primary_age <= CONTEXT_STALE_MS
        requester_is_fresher = requester_context is not None and requester_age < primary_age
        if primary_is_fresh or not requester_is_fresher:
            return primary_context

    if requester_context is not None:
        return requester_context

    if local_context:
        return local_context
    return latest_any_device_context.copy()


async def close_live_session(
    live_bridge: DoubaoRealtimeBridge | None,
    forward_task: asyncio.Task[None] | None,
) -> tuple[DoubaoRealtimeBridge | None, asyncio.Task[None] | None]:
    if forward_task is not None:
        forward_task.cancel()
        try:
            await forward_task
        except asyncio.CancelledError:
            pass
        except Exception:
            pass
        forward_task = None

    if live_bridge is not None:
        try:
            await live_bridge.finish_session()
        except Exception:
            pass
        try:
            await live_bridge.close()
        except Exception:
            pass
        live_bridge = None

    return live_bridge, forward_task


async def forward_audio_session(
    websocket: WebSocket,
    session_id: str,
    live_bridge: DoubaoRealtimeBridge,
    requester_device_id: str,
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
                    if not is_interim and is_vitals_assessment_query(text):
                        await send_backend_message(
                            websocket,
                            BackendMessage(
                                type="backend_status",
                                session_id=session_id,
                                detail="vitals_query_detected",
                            ),
                        )
                        requested_ms = time.time() * 1000.0
                        await send_backend_message(
                            websocket,
                            BackendMessage(
                                type="request_fresh_status",
                                session_id=session_id,
                                detail="vitals_query",
                            ),
                        )
                        await wait_for_fresh_device_context(
                            device_id=requester_device_id,
                            updated_after_ms=requested_ms,
                        )
                        fresh_context = resolve_device_context(
                            requester_device_id=requester_device_id,
                            local_context={},
                        )
                        has_metric = any(
                            fresh_context.get(k) is not None
                            for k in ("heart_rate_bpm", "spo2_percent", "temperature_c")
                        )
                        if not has_metric:
                            await send_backend_message(
                                websocket,
                                BackendMessage(
                                    type="backend_status",
                                    session_id=session_id,
                                    detail="vitals_query_fallback_live_no_metrics",
                                ),
                            )
                            continue

                        await send_backend_message(
                            websocket,
                            BackendMessage(
                                type="backend_status",
                                session_id=session_id,
                                detail="vitals_query_routed_text_round",
                            ),
                        )
                        routed_text_round_sessions.add(session_id)
                        try:
                            await live_bridge.finish_session()
                        except Exception:
                            pass
                        try:
                            await live_bridge.close()
                        except Exception:
                            pass
                        live_bridge = None  # type: ignore[assignment]
                        try:
                            await run_text_round_and_stream(
                                websocket=websocket,
                                session_id=session_id,
                                requester_device_id=requester_device_id,
                                local_context=fresh_context,
                                query=build_vitals_assessment_query(text),
                                tts_end_detail="vitals_text_round_tts_done",
                            )
                        except Exception as exc:
                            await send_backend_message(
                                websocket,
                                BackendMessage(
                                    type="error",
                                    session_id=session_id,
                                    detail=f"vitals_text_round_error: {exc}",
                                ),
                            )
                        finally:
                            routed_text_round_sessions.discard(session_id)
                        break

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
    except Exception as exc:
        await send_backend_message(
            websocket,
            BackendMessage(
                type="error",
                session_id=session_id,
                detail=f"live_session_error: {exc}",
            ),
        )
    finally:
        if live_bridge is not None:
            try:
                await live_bridge.finish_session()
            except Exception:
                pass
            try:
                await live_bridge.close()
            except Exception:
                pass


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
    device_context: dict[str, Any] = {}
    last_orphan_audio_warn_ms = 0.0

    try:
        while True:
            if forward_task is not None and forward_task.done():
                try:
                    await forward_task
                except Exception as exc:
                    await send_backend_message(
                        websocket,
                        BackendMessage(
                            type="error",
                            detail=f"forward_task_stopped: {exc}",
                        ),
                    )
                forward_task = None
                live_bridge = None

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
                if live_bridge is not None or forward_task is not None:
                    live_bridge, forward_task = await close_live_session(
                        live_bridge, forward_task
                    )

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
                    await run_text_round_and_stream(
                        websocket=websocket,
                        session_id=message.session_id or "",
                        requester_device_id=message.device_id,
                        local_context=device_context,
                        query=message.text,
                        tts_end_detail="text_round_tts_done",
                    )
                    continue

                live_bridge = DoubaoRealtimeBridge()
                await live_bridge.connect()
                await live_bridge.start_audio_session(
                    resolve_device_context(
                        requester_device_id=message.device_id,
                        local_context=device_context,
                    )
                )
                await send_backend_message(
                    websocket,
                    BackendMessage(
                        type="session_started",
                        session_id=message.session_id,
                        detail="live_audio_round_start",
                    ),
                )
                forward_task = asyncio.create_task(
                    forward_audio_session(
                        websocket,
                        message.session_id or "",
                        live_bridge,
                        message.device_id,
                    )
                )
                continue

            if message.type == "audio_chunk":
                if live_bridge is not None and message.payload_b64:
                    audio_bytes = base64.b64decode(message.payload_b64)
                    await live_bridge.send_audio_chunk(audio_bytes)
                    continue
                now_ms = time.time() * 1000.0
                if (now_ms - last_orphan_audio_warn_ms) >= 1500.0:
                    last_orphan_audio_warn_ms = now_ms
                    await send_backend_message(
                        websocket,
                        BackendMessage(
                            type="backend_status",
                            session_id=message.session_id,
                            detail="audio_chunk_ignored_no_live_session",
                        ),
                    )
                continue

            if message.type == "interrupt":
                live_bridge, forward_task = await close_live_session(
                    live_bridge, forward_task
                )
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
                if message.session_id and message.session_id in routed_text_round_sessions:
                    await send_backend_message(
                        websocket,
                        BackendMessage(
                            type="backend_status",
                            session_id=message.session_id,
                            detail="session_stop_ack",
                        ),
                    )
                    continue
                if forward_task is not None or live_bridge is not None:
                    live_bridge, forward_task = await close_live_session(
                        live_bridge, forward_task
                    )
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
                now_ms = time.time() * 1000.0
                previous_context = latest_device_context_by_device_id.get(
                    message.device_id, {}
                )
                device_context = {
                    "state": message.state,
                    "heart_rate_bpm": message.heart_rate_bpm,
                    "spo2_percent": message.spo2_percent,
                    "temperature_c": message.temperature_c,
                    "fall_state": message.fall_state,
                    "signal_quality": message.signal_quality,
                    "finger_detected": message.finger_detected,
                    "device_source": message.device_source,
                    "temperature_source": message.temperature_source,
                    "measurement_confidence": message.measurement_confidence,
                    "temperature_validity": message.temperature_validity,
                    "_updated_at_ms": now_ms,
                }

                prev_updated_at = previous_context.get("_updated_at_ms")
                prev_is_fresh = isinstance(prev_updated_at, (int, float)) and (
                    now_ms - float(prev_updated_at) <= METRIC_HOLD_MS
                )
                if prev_is_fresh:
                    for metric_key in ("heart_rate_bpm", "spo2_percent", "temperature_c"):
                        if device_context.get(metric_key) is None:
                            prev_metric = previous_context.get(metric_key)
                            if prev_metric is not None:
                                device_context[metric_key] = prev_metric

                latest_device_context_by_device_id[message.device_id] = device_context.copy()
                context_update_events.setdefault(message.device_id, asyncio.Event()).set()
                # Keep bounded memory even if many transient device_ids connect over time.
                if len(latest_device_context_by_device_id) > MAX_DEVICE_CONTEXTS:
                    oldest_device_id = min(
                        latest_device_context_by_device_id.items(),
                        key=lambda item: item[1].get("_updated_at_ms", 0),
                    )[0]
                    latest_device_context_by_device_id.pop(oldest_device_id, None)
                latest_any_device_context.clear()
                latest_any_device_context.update(device_context)
                await send_backend_message(
                    websocket,
                    BackendMessage(
                        type="backend_status",
                        session_id=message.session_id,
                        detail=(
                            f"device_state={message.state or 'unknown'}"
                            f"/hr={message.heart_rate_bpm if message.heart_rate_bpm is not None else '-'}"
                            f"/spo2={message.spo2_percent if message.spo2_percent is not None else '-'}"
                            f"/temp={message.temperature_c if message.temperature_c is not None else '-'}"
                            f"/src={message.device_source or '-'}"
                            f"/conf={message.measurement_confidence or '-'}"
                        ),
                    ),
                )
                continue

    except WebSocketDisconnect:
        await close_live_session(live_bridge, forward_task)
        return
