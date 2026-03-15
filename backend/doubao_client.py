from __future__ import annotations

import asyncio
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import websockets

from config import settings
from protocol import EventId, ParsedPacket, pack_audio_event, pack_json_event, parse_packet


@dataclass
class DoubaoSessionConfig:
    model: str
    speaker: str
    sample_rate: int = 16000


class DoubaoRealtimeBridge:
    def __init__(self) -> None:
        self.ws_url = settings.doubao_ws_url
        self.app_id = settings.doubao_app_id
        self.access_key = settings.doubao_access_key
        self.secret_key = settings.doubao_secret_key
        self.resource_id = settings.doubao_resource_id
        self.app_key = settings.doubao_app_key
        self.default_session = DoubaoSessionConfig(
            model=settings.doubao_model,
            speaker=settings.doubao_speaker,
        )
        self._ws: Any | None = None
        self._connect_id: str | None = None
        self._session_id: str | None = None

    def credentials_ready(self) -> bool:
        return bool(self.app_id and self.access_key)

    def _format_device_context(self, device_context: dict[str, Any] | None) -> str:
        if not device_context:
            return (
                "Current device context: no live health metrics attached yet. "
                "If the user asks about heart rate, blood oxygen, temperature, or fall alerts, "
                "explain that current live values are unavailable unless the device provides them."
            )

        parts: list[str] = []

        voice_state = device_context.get("state")
        if voice_state:
            parts.append(f"voice_state={voice_state}")

        hr = device_context.get("heart_rate_bpm")
        if hr is not None:
            parts.append(f"heart_rate_bpm={int(round(hr))}")

        spo2 = device_context.get("spo2_percent")
        if spo2 is not None:
            parts.append(f"spo2_percent={int(round(spo2))}")

        temp = device_context.get("temperature_c")
        if temp is not None:
            parts.append(f"temperature_c={temp:.1f}")

        fall_state = device_context.get("fall_state")
        if fall_state:
            parts.append(f"fall_state={fall_state}")

        quality = device_context.get("signal_quality")
        if quality:
            parts.append(f"signal_quality={quality}")

        finger_detected = device_context.get("finger_detected")
        if finger_detected is not None:
            parts.append(f"finger_detected={finger_detected}")

        measurement_confidence = device_context.get("measurement_confidence")
        if measurement_confidence:
            parts.append(f"measurement_confidence={measurement_confidence}")

        temperature_validity = device_context.get("temperature_validity")
        if temperature_validity:
            parts.append(f"temperature_validity={temperature_validity}")

        device_source = device_context.get("device_source")
        if device_source:
            parts.append(f"device_source={device_source}")

        temperature_source = device_context.get("temperature_source")
        if temperature_source:
            parts.append(f"temperature_source={temperature_source}")

        if not parts:
            return "Current device context: device is connected, but no specific health metrics are available yet."

        return "Current device context: " + "; ".join(parts) + "."

    def _build_system_role(self, device_context: dict[str, Any] | None) -> str:
        return (
            "You are An Xiaoning, a friendly Chinese-speaking health assistant running on an ESP32 health monitoring device. "
            "Always answer in concise, natural Chinese suitable for voice playback. "
            "Keep most answers within one to three short sentences. "
            "When the user asks who you are, answer a bit more fully: explain that you are An Xiaoning, their health assistant, "
            "and that you can help explain heart rate, blood oxygen, temperature, fall alerts, and daily health questions. "
            "Do not call yourself Doubao or a generic language model unless the user explicitly asks about the underlying technology. "
            "Do not invent sensor values that were not provided. "
            "If current live values are not available, say so directly and ask the user to provide the displayed values if needed. "
            "When finger_detected=true, do not say the finger is missing. "
            "If measurement_confidence is low, say the signal quality is weak and suggest retesting before giving a strong conclusion. "
            "If measurement_confidence is low but heart_rate_bpm or spo2_percent is present, you may quote them as approximate on-screen values with a clear caution. "
            "If measurement_confidence is invalid, do not interpret heart rate or blood oxygen as a reliable current result. "
            "If temperature_validity is not body_screening, do not treat the temperature as a trustworthy body temperature reading; explain that it may reflect environment, surface, or device heat and suggest retesting. "
            "For high-risk situations such as chest pain, severe breathing difficulty, sustained high fever, or very low blood oxygen, "
            "clearly advise seeking medical attention promptly. "
            f"{self._format_device_context(device_context)}"
        )

    def start_session_payload(self, device_context: dict[str, Any] | None = None) -> dict:
        return {
            "asr": {
                "audio_info": {
                    "format": "pcm",
                    "sample_rate": self.default_session.sample_rate,
                    "channel": 1,
                }
            },
            "tts": {
                "speaker": self.default_session.speaker,
                "audio_config": {
                    "channel": 1,
                    "format": "pcm_s16le",
                    "sample_rate": 24000,
                },
            },
            "dialog": {
                "bot_name": "AnXiaoning",
                "system_role": self._build_system_role(device_context),
                "extra": {
                    "model": self.default_session.model,
                },
            },
        }

    async def ensure_ready(self) -> None:
        if not self.credentials_ready():
            raise RuntimeError(
                "Doubao credentials are missing. Fill DOUBAO_APP_ID and "
                "DOUBAO_ACCESS_KEY in backend/.env first."
            )

    def _headers(self) -> dict[str, str]:
        return {
            "X-Api-App-ID": self.app_id,
            "X-Api-Access-Key": self.access_key,
            "X-Api-Resource-Id": self.resource_id,
            "X-Api-App-Key": self.app_key,
            "X-Api-Connect-Id": self._connect_id or "",
        }

    async def connect(self) -> None:
        await self.ensure_ready()
        self._connect_id = str(uuid.uuid4())
        self._ws = await websockets.connect(
            self.ws_url,
            additional_headers=self._headers(),
            max_size=None,
        )
        await self._ws.send(
            pack_json_event(
                EventId.START_CONNECTION,
                {},
            )
        )
        packet = await self.recv_packet()
        if packet.event != EventId.CONNECTION_STARTED:
            raise RuntimeError(
                "Unexpected connect response: "
                f"message_type={packet.message_type} code={packet.code} "
                f"event={packet.event} payload_json={packet.payload_json} "
                f"payload_len={len(packet.payload)}"
            )

    async def close(self) -> None:
        if self._ws is not None:
            try:
                await self._ws.send(
                    pack_json_event(
                        EventId.FINISH_CONNECTION,
                        {},
                    )
                )
                await asyncio.sleep(0.1)
            finally:
                await self._ws.close()
                self._ws = None

    async def start_text_session(self, device_context: dict[str, Any] | None = None) -> str:
        if self._ws is None:
            raise RuntimeError("Doubao websocket is not connected")

        self._session_id = str(uuid.uuid4())
        payload = self.start_session_payload(device_context)
        payload["dialog"]["extra"]["input_mod"] = "text"
        await self._ws.send(
            pack_json_event(
                EventId.SESSION_START,
                payload,
                session_id=self._session_id,
            )
        )
        packet = await self.recv_packet()
        if packet.event != EventId.SESSION_STARTED:
            raise RuntimeError(
                "Unexpected session start response: "
                f"message_type={packet.message_type} code={packet.code} "
                f"event={packet.event} payload_json={packet.payload_json} "
                f"payload_len={len(packet.payload)}"
            )
        return self._session_id

    async def start_audio_session(self, device_context: dict[str, Any] | None = None) -> str:
        if self._ws is None:
            raise RuntimeError("Doubao websocket is not connected")

        self._session_id = str(uuid.uuid4())
        payload = self.start_session_payload(device_context)
        await self._ws.send(
            pack_json_event(
                EventId.SESSION_START,
                payload,
                session_id=self._session_id,
            )
        )
        packet = await self.recv_packet()
        if packet.event != EventId.SESSION_STARTED:
            raise RuntimeError(
                "Unexpected audio session start response: "
                f"message_type={packet.message_type} code={packet.code} "
                f"event={packet.event} payload_json={packet.payload_json} "
                f"payload_len={len(packet.payload)}"
            )
        return self._session_id

    async def send_text_query(self, text: str) -> None:
        if self._ws is None or self._session_id is None:
            raise RuntimeError("Text session is not started")

        await self._ws.send(
            pack_json_event(
                EventId.CHAT_TEXT_QUERY,
                {"content": text},
                session_id=self._session_id,
            )
        )

    async def send_audio_chunk(self, audio_bytes: bytes) -> None:
        if self._ws is None or self._session_id is None:
            raise RuntimeError("Audio session is not started")
        await self._ws.send(
            pack_audio_event(
                EventId.TASK_REQUEST,
                audio_bytes,
                session_id=self._session_id,
            )
        )

    async def finish_session(self) -> None:
        if self._ws is None or self._session_id is None:
            return
        await self._ws.send(
            pack_json_event(
                EventId.SESSION_FINISH,
                {},
                session_id=self._session_id,
            )
        )
        self._session_id = None

    async def recv_packet(self) -> ParsedPacket:
        if self._ws is None:
            raise RuntimeError("Doubao websocket is not connected")
        data = await self._ws.recv()
        if not isinstance(data, bytes):
            raise RuntimeError("Expected binary packet from Doubao")
        return parse_packet(data)

    async def collect_text_round(
        self,
        *,
        query: str,
        output_audio_path: Path | None = None,
        timeout_s: float = 20.0,
    ) -> dict[str, Any]:
        if self._session_id is None:
            raise RuntimeError("Session must be started before sending a query")

        await self.send_text_query(query)
        audio_chunks: list[bytes] = []
        asr_final = ""
        reply_text = ""
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout_s

        while True:
            remaining = deadline - loop.time()
            if remaining <= 0:
                raise TimeoutError("Timed out waiting for Doubao response")

            packet = await asyncio.wait_for(self.recv_packet(), timeout=remaining)

            if packet.event == EventId.ASR_RESPONSE and packet.payload_json:
                results = packet.payload_json.get("results", [])
                if results:
                    asr_final = results[-1].get("text", asr_final)

            elif packet.event == EventId.CHAT_RESPONSE and packet.payload_json:
                reply_text += packet.payload_json.get("content", "")

            elif packet.event == EventId.TTS_RESPONSE:
                audio_chunks.append(packet.payload)

            elif packet.event == EventId.TTS_ENDED:
                break

            elif packet.event in {EventId.SESSION_FAILED, EventId.DIALOG_COMMON_ERROR}:
                raise RuntimeError(str(packet.payload_json))

        if output_audio_path is not None and audio_chunks:
            output_audio_path.write_bytes(b"".join(audio_chunks))

        return {
            "asr_text": asr_final,
            "reply_text": reply_text,
            "audio_bytes": sum(len(chunk) for chunk in audio_chunks),
            "audio_data": b"".join(audio_chunks),
        }
