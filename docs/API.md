# HTTP API contract

Base URL is configured on the server. All `/v1/*` routes accept `Authorization: Bearer <DEVICE_AUTH_TOKEN>` when the server has a non-empty `DEVICE_AUTH_TOKEN`; auth is disabled in local mock mode by default. Requests are capped by `MAX_AUDIO_BYTES` (default 10 MiB). Provider operations time out per `REQUEST_TIMEOUT_SECONDS`.

| Route | Request | Response |
| --- | --- | --- |
| `GET /health` | none | status, provider mode, supported input/output rates |
| `POST /v1/transcribe` | multipart `file`: non-empty RIFF/WAVE, mono, 16 kHz, 16-bit PCM | JSON `{transcript}` |
| `POST /v1/chat` | JSON `{text, conversation_id?, device_id?}` | JSON transcript and response text |
| `POST /v1/speech` | JSON `{text}` | raw 24 kHz mono s16le PCM; `X-Audio-Format`, `X-Sample-Rate`, `X-Channels` |
| `POST /v1/voice` | multipart `file`, optional `conversation_id`, `device_id` | JSON transcript, response, base64 raw PCM and audio metadata |
| `POST /v1/voice/audio` | same multipart fields | raw PCM for embedded clients; URL-encoded transcript/response and audio metadata in headers |

Errors use FastAPI JSON `{detail: ...}`. Invalid WAV/parameters return 422, oversized audio 413, bad token 401, provider timeout 504, and provider failure 502. A generated conversation ID is returned for clients that omit one. IDs are scoped in SQLite but are not access-control boundaries: use device authentication and network controls.

The robot output format is raw signed 16-bit little-endian, mono, 24,000 samples/second (not WAV). Input is a standard 44-byte PCM WAV header plus signed 16-bit little-endian mono samples at 16,000 samples/second. The firmware uses `POST /v1/voice/audio` so response audio can be streamed without base64 expansion.
