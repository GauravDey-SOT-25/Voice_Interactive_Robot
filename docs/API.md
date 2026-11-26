# API contract

Base URL is configured on the server. All `/v1/*` routes accept `Authorization: Bearer <DEVICE_AUTH_TOKEN>` when the server has a non-empty `DEVICE_AUTH_TOKEN`; auth is disabled in local mock mode by default. Requests are capped by `MAX_AUDIO_BYTES` (default 10 MiB). Provider operations time out per `REQUEST_TIMEOUT_SECONDS`.

| Route | Request | Response |
| --- | --- | --- |
| `GET /health` | none | status, provider mode, supported input/output rates |
| `POST /v1/transcribe` | multipart `file`: non-empty RIFF/WAVE, mono, 16 kHz, 16-bit PCM | JSON `{transcript}` |
| `POST /v1/chat` | JSON `{text, conversation_id?, device_id?}` | JSON transcript and response text |
| `POST /v1/speech` | JSON `{text}` | raw 24 kHz mono s16le PCM; `X-Audio-Format`, `X-Sample-Rate`, `X-Channels` |
| `POST /v1/voice` | multipart `file`, optional `conversation_id`, `device_id` | JSON transcript, response, base64 raw PCM and audio metadata |
| `POST /v1/voice/audio` | same multipart fields | raw PCM for embedded clients; URL-encoded transcript/response and audio metadata in headers |
| `WS /v1/voice/live` | bearer auth header; binary mic PCM and JSON controls | provider status/events as JSON and generated PCM as binary frames |

Errors use FastAPI JSON `{detail: ...}`. Invalid WAV/parameters return 422, oversized audio 413, bad token 401, provider timeout 504, and provider failure 502. A generated conversation ID is returned for clients that omit one. IDs are scoped in SQLite but are not access-control boundaries: use device authentication and network controls.

In `AI_MODE=mock`, the transcription/reply values are deterministic and speech output is a short 660 Hz test tone followed by silence, rather than synthesized words.

## Live speech WebSocket

Connect to `/v1/voice/live?provider=auto`. `auto` tries the configured provider order at session startup; the default is `openai_realtime,gemini_live`. If the server is in `AI_MODE=mock`, it falls back to the local `mock_live` protocol fixture when no cloud provider is configured. The mock emits a tone and does not validate a speech model. For OpenAI Realtime, silence termination is controlled by `LIVE_SILENCE_DURATION_MS` (default 800 ms), `LIVE_VAD_THRESHOLD` (0.5), and `LIVE_PREFIX_PADDING_MS` (300 ms). Gemini Live uses the model's server-side turn detection defaults.

Send a JSON control message at connect:

```json
{"type":"start","format":"s16le","sample_rate":16000,"channels":1}
```

Then send continuous binary frames containing mono signed 16-bit little-endian PCM at 16 kHz. Each frame must be no larger than 8192 bytes and contain complete samples. The server sends status and transcript messages as JSON text and generated mono PCM16 at 24 kHz as binary frames. Send `{"type":"interrupt"}` after local VAD confirms barge-in; send `{"type":"close"}` to end the session. The firmware sends frames continuously so provider VAD can detect pauses and turn boundaries.

The backend has adapters for OpenAI Realtime and Gemini Live. It selects only providers with credentials configured and can fall back if session setup fails. `scripts/live_provider_smoke.py` tests every configured live adapter using the spoken WAV named by `ROBOT_LIVE_TEST_WAV` (mono PCM16 at 16 kHz). Real model checks incur provider usage. No provider keys were configured during the current implementation run, so cloud model E2E results are pending; the local WebSocket protocol fixture was tested.

The older `POST /v1/voice/audio` endpoint remains for turn-based clients. Its input is a standard PCM WAV at 16 kHz, and its output is raw signed 16-bit little-endian mono PCM at 24 kHz.
