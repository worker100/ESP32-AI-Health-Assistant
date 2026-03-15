from __future__ import annotations

import json
import struct
from dataclasses import dataclass
from enum import IntEnum
from typing import Any


class MessageType(IntEnum):
    FULL_CLIENT_REQUEST = 0x1
    AUDIO_ONLY_REQUEST = 0x2
    FULL_SERVER_RESPONSE = 0x9
    AUDIO_ONLY_RESPONSE = 0xB
    ERROR_INFORMATION = 0xF


class Serialization(IntEnum):
    RAW = 0x0
    JSON = 0x1


class Compression(IntEnum):
    NONE = 0x0


class EventId(IntEnum):
    START_CONNECTION = 1
    FINISH_CONNECTION = 2
    SESSION_START = 100
    SESSION_FINISH = 102
    TASK_REQUEST = 200
    CHAT_TTS_TEXT = 500
    CHAT_TEXT_QUERY = 501
    CONNECTION_STARTED = 50
    CONNECTION_FAILED = 51
    CONNECTION_FINISHED = 52
    SESSION_STARTED = 150
    SESSION_FINISHED = 152
    SESSION_FAILED = 153
    USAGE_RESPONSE = 154
    TTS_SENTENCE_START = 350
    TTS_SENTENCE_END = 351
    TTS_RESPONSE = 352
    TTS_ENDED = 359
    ASR_INFO = 450
    ASR_RESPONSE = 451
    ASR_ENDED = 459
    CHAT_RESPONSE = 550
    CHAT_TEXT_QUERY_CONFIRMED = 553
    CHAT_ENDED = 559
    DIALOG_COMMON_ERROR = 599


HEADER = bytes([0x11, 0x00, 0x10, 0x00])
FLAG_EVENT_PRESENT = 0x4


def _read_u32(data: bytes, offset: int, label: str) -> tuple[int, int]:
    if offset + 4 > len(data):
        raise ValueError(f"Packet too short while reading {label}")
    value = struct.unpack(">I", data[offset : offset + 4])[0]
    return value, offset + 4


def _read_bytes(data: bytes, offset: int, size: int, label: str) -> tuple[bytes, int]:
    if size < 0:
        raise ValueError(f"Invalid negative size for {label}")
    end = offset + size
    if end > len(data):
        raise ValueError(f"Packet too short while reading {label} payload")
    return data[offset:end], end


@dataclass
class ParsedPacket:
    message_type: int
    flags: int
    serialization: int
    compression: int
    code: int | None
    event: int | None
    session_id: str | None
    payload: bytes
    payload_json: dict[str, Any] | None


def _build_header(message_type: MessageType, flags: int, serialization: Serialization) -> bytes:
    return bytes(
        [
            HEADER[0],
            ((int(message_type) & 0x0F) << 4) | (flags & 0x0F),
            ((int(serialization) & 0x0F) << 4) | int(Compression.NONE),
            HEADER[3],
        ]
    )


def _pack_event_payload(
    message_type: MessageType,
    event: EventId,
    payload: bytes,
    *,
    session_id: str | None = None,
    connect_id: str | None = None,
    serialization: Serialization = Serialization.JSON,
) -> bytes:
    frame = bytearray()
    frame.extend(_build_header(message_type, FLAG_EVENT_PRESENT, serialization))
    frame.extend(struct.pack(">I", int(event)))

    if connect_id is not None:
        connect_id_bytes = connect_id.encode("utf-8")
        frame.extend(struct.pack(">I", len(connect_id_bytes)))
        frame.extend(connect_id_bytes)

    if session_id is not None:
        session_id_bytes = session_id.encode("utf-8")
        frame.extend(struct.pack(">I", len(session_id_bytes)))
        frame.extend(session_id_bytes)

    frame.extend(struct.pack(">I", len(payload)))
    frame.extend(payload)
    return bytes(frame)


def pack_json_event(
    event: EventId,
    payload_json: dict[str, Any],
    *,
    session_id: str | None = None,
    connect_id: str | None = None,
) -> bytes:
    payload = json.dumps(payload_json, ensure_ascii=False).encode("utf-8")
    return _pack_event_payload(
        MessageType.FULL_CLIENT_REQUEST,
        event,
        payload,
        session_id=session_id,
        connect_id=connect_id,
        serialization=Serialization.JSON,
    )


def pack_audio_event(
    event: EventId,
    payload: bytes,
    *,
    session_id: str,
) -> bytes:
    return _pack_event_payload(
        MessageType.AUDIO_ONLY_REQUEST,
        event,
        payload,
        session_id=session_id,
        serialization=Serialization.RAW,
    )


def parse_packet(data: bytes) -> ParsedPacket:
    if len(data) < 8:
        raise ValueError("Packet too short")

    version_and_size = data[0]
    if version_and_size != 0x11:
        raise ValueError(f"Unexpected protocol header byte: 0x{version_and_size:02X}")

    message_type = (data[1] >> 4) & 0x0F
    flags = data[1] & 0x0F
    serialization = (data[2] >> 4) & 0x0F
    compression = data[2] & 0x0F
    offset = 4

    code = None
    event = None
    session_id = None

    if message_type == MessageType.ERROR_INFORMATION:
        code, offset = _read_u32(data, offset, "error code")

    if flags & FLAG_EVENT_PRESENT:
        event, offset = _read_u32(data, offset, "event id")

    if event not in {
        EventId.START_CONNECTION,
        EventId.FINISH_CONNECTION,
        EventId.CONNECTION_STARTED,
        EventId.CONNECTION_FAILED,
        EventId.CONNECTION_FINISHED,
        None,
    }:
        session_size, offset = _read_u32(data, offset, "session id size")
        if session_size > 0:
            session_raw, offset = _read_bytes(data, offset, session_size, "session id")
            session_id = session_raw.decode("utf-8")

    payload_size, offset = _read_u32(data, offset, "payload size")
    payload, offset = _read_bytes(data, offset, payload_size, "payload")

    payload_json = None
    if serialization == Serialization.JSON and payload:
        try:
            payload_json = json.loads(payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            payload_json = None

    return ParsedPacket(
        message_type=message_type,
        flags=flags,
        serialization=serialization,
        compression=compression,
        code=code,
        event=event,
        session_id=session_id,
        payload=payload,
        payload_json=payload_json,
    )
