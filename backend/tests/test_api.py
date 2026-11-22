import io
import wave

from fastapi.testclient import TestClient

from backend.app import create_app
from backend.config import Settings
from backend.providers import MockProvider


def make_wav():
    data = io.BytesIO()
    with wave.open(data, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(16_000)
        wav.writeframes(b"\x00\x00" * 1600)
    return data.getvalue()


def make_client(tmp_path, token=""):
    settings = Settings(database_path=str(tmp_path / "conversations.sqlite3"), device_auth_token=token)
    return TestClient(create_app(settings, MockProvider()))


def test_health_chat_history_and_speech(tmp_path):
    with make_client(tmp_path) as client:
        assert client.get("/health").json()["status"] == "ok"
        first = client.post("/v1/chat", json={"text": "Hello", "conversation_id": "c1"})
        assert first.status_code == 200
        assert first.json()["response_text"] == "I heard you say: Hello"
        second = client.post("/v1/chat", json={"text": "Again", "conversation_id": "c1"})
        assert second.status_code == 200
        speech = client.post("/v1/speech", json={"text": "Hello"})
        assert len(speech.content) == 7200 * 2
        assert speech.content != b"\x00" * len(speech.content)
        assert speech.headers["x-sample-rate"] == "24000"


def test_voice_endpoints_contract_and_audio_validation(tmp_path):
    with make_client(tmp_path) as client:
        response = client.post("/v1/voice", files={"file": ("voice.wav", make_wav(), "audio/wav")})
        assert response.status_code == 200
        body = response.json()
        assert body["transcript"] == "Hello robot"
        assert body["sample_rate"] == 24_000
        robot = client.post("/v1/voice/audio", files={"file": ("voice.wav", make_wav(), "audio/wav")})
        assert robot.content != b"\x00" * len(robot.content)
        assert robot.headers["x-audio-format"] == "s16le"
        invalid = client.post("/v1/transcribe", files={"file": ("bad.wav", b"bad", "audio/wav")})
        assert invalid.status_code == 422


def test_device_auth_is_enforced(tmp_path):
    with make_client(tmp_path, token="device-secret") as client:
        assert client.post("/v1/chat", json={"text": "Hello"}).status_code == 401
        ok = client.post(
            "/v1/chat", json={"text": "Hello"}, headers={"Authorization": "Bearer device-secret"}
        )
        assert ok.status_code == 200
