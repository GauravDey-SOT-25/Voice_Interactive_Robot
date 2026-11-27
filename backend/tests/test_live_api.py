from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect

from backend.app import create_app
from backend.config import Settings
from backend.live_providers import resample_pcm16
from backend.providers import MockProvider


def make_client(tmp_path, token=""):
    settings = Settings(
        database_path=str(tmp_path / "live.sqlite3"),
        device_auth_token=token,
        ai_mode="mock",
        openai_api_key="",
        google_api_key="",
    )
    return TestClient(create_app(settings, MockProvider()))


def test_live_health_reports_mock_protocol_fixture(tmp_path):
    with make_client(tmp_path) as client:
        health = client.get("/health").json()
        assert "mock_live" in health["live_providers_configured"]
        assert health["audio"] == {
            "input_rate": 16000,
            "output_rate": 24000,
            "format": "s16le/mono",
        }


def test_live_websocket_streams_pcm_both_directions(tmp_path):
    with make_client(tmp_path) as client, client.websocket_connect(
        "/v1/voice/live?provider=mock_live"
    ) as socket:
            assert socket.receive_json()["type"] == "connecting"
            started = socket.receive_json()
            assert started["type"] == "session_started"
            assert started["provider"] == "mock_live"
            socket.send_bytes(b"\x01\x00" * 512)
            assert socket.receive_json()["type"] == "mock_response"
            audio = socket.receive_bytes()
            assert len(audio) == 4800
            assert audio != b"\x00" * len(audio)
            assert socket.receive_json()["type"] == "response_complete"
            socket.send_json({"type": "close"})


def test_live_auto_selects_offline_mock_in_mock_mode(tmp_path):
    with make_client(tmp_path) as client, client.websocket_connect("/v1/voice/live") as socket:
        assert socket.receive_json()["type"] == "connecting"
        started = socket.receive_json()
        assert started["provider"] == "mock_live"
        socket.send_json({"type": "close"})


def test_live_websocket_requires_device_token(tmp_path):
    with make_client(tmp_path, token="secret") as client:
        try:
            with client.websocket_connect("/v1/voice/live?provider=mock_live"):
                raise AssertionError("unauthorized WebSocket unexpectedly connected")
        except WebSocketDisconnect as exc:
            assert exc.code == 4401


def test_pcm_resampling_expands_16khz_frames_to_24khz():
    assert len(resample_pcm16(b"\x01\x00" * 160, 16_000, 24_000)) == 480
