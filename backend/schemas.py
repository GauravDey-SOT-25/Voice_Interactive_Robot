from pydantic import BaseModel, Field


class ChatRequest(BaseModel):
    text: str = Field(min_length=1, max_length=4000)
    conversation_id: str = Field(default="default", min_length=1, max_length=128)
    device_id: str = Field(default="local-test", min_length=1, max_length=128)


class ChatResponse(BaseModel):
    conversation_id: str
    transcript: str
    response_text: str


class VoiceResponse(ChatResponse):
    audio: str
    audio_format: str = "s16le"
    sample_rate: int = 24_000
    channels: int = 1
    audio_bytes: int


class SpeechRequest(BaseModel):
    text: str = Field(min_length=1, max_length=4000)
