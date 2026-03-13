from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", env_file_encoding="utf-8")

    backend_host: str = Field(default="127.0.0.1", alias="BACKEND_HOST")
    backend_port: int = Field(default=8787, alias="BACKEND_PORT")
    device_ws_path: str = Field(default="/ws/device", alias="DEVICE_WS_PATH")

    doubao_ws_url: str = Field(
        default="wss://openspeech.bytedance.com/api/v3/realtime/dialogue",
        alias="DOUBAO_WS_URL",
    )
    doubao_app_id: str = Field(default="", alias="DOUBAO_APP_ID")
    doubao_access_key: str = Field(default="", alias="DOUBAO_ACCESS_KEY")
    doubao_secret_key: str = Field(default="", alias="DOUBAO_SECRET_KEY")
    doubao_resource_id: str = Field(
        default="volc.speech.dialog", alias="DOUBAO_RESOURCE_ID"
    )
    doubao_app_key: str = Field(
        default="PlgvMymc7f3tQnJ6", alias="DOUBAO_APP_KEY"
    )
    doubao_model: str = Field(default="1.2.1.0", alias="DOUBAO_MODEL")
    doubao_speaker: str = Field(
        default="zh_female_vv_jupiter_bigtts", alias="DOUBAO_SPEAKER"
    )


settings = Settings()
