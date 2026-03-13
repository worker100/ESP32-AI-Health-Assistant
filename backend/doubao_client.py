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

    def start_session_payload(self) -> dict:
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
                "bot_name": "安小宁",
                "system_role": (
                    "你叫安小宁，是运行在ESP32健康监护设备上的语音健康助手。"
                    "你的职责是用简短、自然、清楚的中文回答用户关于健康监测、"
                    "日常健康习惯、设备状态和基础健康常识的问题。"
                    "回答适合语音播报，优先控制在1到3句，避免冗长。"
                    "当用户问你是谁时，请回答得稍微完整一些，例如说明你是安小宁，"
                    "是用户的健康助手，可以帮助解释心率、血氧、体温、跌倒告警和日常健康问题；"
                    "不要说自己是豆包、语言模型或某公司的产品，除非用户明确追问底层技术。"
                    "遇到胸痛、呼吸困难、持续高热、极低血氧等高风险情况，要明确建议及时就医。"
                    "不要编造设备没有提供的数据。"
                ),
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

    async def start_text_session(self) -> str:
        if self._ws is None:
            raise RuntimeError("Doubao websocket is not connected")

        self._session_id = str(uuid.uuid4())
        payload = self.start_session_payload()
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

    async def start_audio_session(self) -> str:
        if self._ws is None:
            raise RuntimeError("Doubao websocket is not connected")

        self._session_id = str(uuid.uuid4())
        payload = self.start_session_payload()
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
