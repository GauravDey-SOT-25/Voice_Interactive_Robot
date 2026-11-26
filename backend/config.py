from functools import lru_cache

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    app_env: str = "development"
    database_path: str = "./data/conversations.sqlite3"
    device_auth_token: str = ""
    max_audio_bytes: int = 10 * 1024 * 1024
    request_timeout_seconds: float = 45.0
    ai_mode: str = "mock"
    openai_api_key: str = ""
    openai_model: str = "gpt-4o-mini"
    openai_stt_model: str = "gpt-transcribe"
    openai_tts_model: str = "gpt-4o-mini-tts"
    openai_tts_voice: str = "alloy"
    live_provider_order: str = "openai_realtime,gemini_live"
    openai_realtime_model: str = "gpt-realtime-2.1-mini"
    openai_realtime_voice: str = "marin"
    google_api_key: str = ""
    gemini_live_model: str = "gemini-3.8-live"
    live_session_timeout_seconds: float = 20.0
    live_silence_duration_ms: int = 800
    live_vad_threshold: float = 0.5
    live_prefix_padding_ms: int = 300
    system_prompt: str = (
        "You are a friendly, concise voice assistant inside a small interactive robot. "
        "Speak naturally and keep answers brief."
    )

    model_config = SettingsConfigDict(env_file=".env", env_file_encoding="utf-8", extra="ignore")


@lru_cache
def get_settings() -> Settings:
    return Settings()
