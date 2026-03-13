from typing import Literal, Optional

from pydantic import BaseModel, Field


DeviceMessageType = Literal[
    "hello",
    "start_session",
    "audio_chunk",
    "stop_session",
    "interrupt",
    "device_status",
]


class DeviceMessage(BaseModel):
    type: DeviceMessageType
    device_id: str = Field(default="esp32-s3")
    session_id: Optional[str] = None
    sample_rate: Optional[int] = None
    payload_b64: Optional[str] = None
    text: Optional[str] = None
    state: Optional[str] = None


class BackendMessage(BaseModel):
    type: str
    session_id: Optional[str] = None
    text: Optional[str] = None
    detail: Optional[str] = None
    payload_b64: Optional[str] = None
    sample_rate: Optional[int] = None
