# Architecture

```mermaid
flowchart LR
  Mic[INMP441] -->|I2S capture 16 kHz| MCU[T4-S3 half-duplex firmware]
  MCU -->|HTTPS multipart WAV| API[FastAPI]
  API --> STT[STT provider]
  STT --> Store[(SQLite history)]
  Store --> LLM[LLM provider]
  LLM --> TTS[TTS provider]
  TTS -->|raw PCM 24 kHz| MCU
  MCU -->|I2S playback| Amp[MAX98357A + speaker]
```

`VoiceProvider` is the service boundary for transcription, chat completion, and speech synthesis. `MockProvider` is deterministic and offline. `OpenAIProvider` uses the official OpenAI Python SDK, with provider keys kept on the server. The API owns validation and conversation orchestration; firmware has no provider-specific logic. Wake word, VAD, streaming capture, interruption handling and acoustic echo cancellation are follow-on work.
