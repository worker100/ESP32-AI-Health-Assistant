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
    heart_rate_bpm: Optional[float] = None
    spo2_percent: Optional[float] = None
    temperature_c: Optional[float] = None
    fall_state: Optional[str] = None
    signal_quality: Optional[str] = None
    finger_detected: Optional[bool] = None
    device_source: Optional[str] = None
    temperature_source: Optional[str] = None
    measurement_confidence: Optional[str] = None
    temperature_validity: Optional[str] = None


class BackendMessage(BaseModel):
    type: str
    session_id: Optional[str] = None
    text: Optional[str] = None
    detail: Optional[str] = None
    payload_b64: Optional[str] = None
    sample_rate: Optional[int] = None
