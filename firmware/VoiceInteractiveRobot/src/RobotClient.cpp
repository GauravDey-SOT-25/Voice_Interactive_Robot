#include "../include/RobotClient.h"

#include "../include/AudioEngine.h"
#include "../include/RobotSecrets.h"

#include <WiFi.h>
#include <esp_heap_caps.h>

#ifndef ROBOT_LIVE_WS_HOST
#define ROBOT_LIVE_WS_HOST "robot-api.example.com"
#endif
#ifndef ROBOT_LIVE_WS_PORT
#define ROBOT_LIVE_WS_PORT 443
#endif
#ifndef ROBOT_LIVE_WS_PATH
#define ROBOT_LIVE_WS_PATH "/v1/voice/live?provider=auto"
#endif

namespace {
constexpr size_t kAudioQueueDepth = 8;
constexpr size_t kMaxSocketAudioBytes = 8192;
constexpr char kStartMessage[] =
    "{\"type\":\"start\",\"format\":\"s16le\",\"sample_rate\":16000,\"channels\":1}";
constexpr char kInterruptMessage[] = "{\"type\":\"interrupt\"}";
}

RobotClient* RobotClient::activeClient_ = nullptr;

bool RobotClient::connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ROBOT_WIFI_SSID, ROBOT_WIFI_PASSWORD);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

bool RobotClient::startLive(AudioEngine& audio) {
  audio_ = &audio;
  activeClient_ = this;
  inputQueue_ = xQueueCreate(kAudioQueueDepth, sizeof(AudioFrame));
  if (inputQueue_ == nullptr) return false;
  socket_.onEvent(socketEvent);
  socket_.setReconnectInterval(3000);
  socket_.enableHeartbeat(15000, 5000, 2);
  if (xTaskCreatePinnedToCore(networkTask, "voice_live_ws", 12288, this, 4, nullptr, 0) != pdPASS) {
    vQueueDelete(inputQueue_);
    inputQueue_ = nullptr;
    return false;
  }
  return true;
}

bool RobotClient::sendAudio(const int16_t* samples, size_t frameCount) {
  if (inputQueue_ == nullptr || samples == nullptr || frameCount == 0 || frameCount > 1024) return false;
  AudioFrame frame = {};
  frame.frames = static_cast<uint16_t>(frameCount);
  memcpy(frame.samples, samples, frameCount * sizeof(int16_t));
  if (xQueueSend(inputQueue_, &frame, 0) == pdTRUE) return true;

  // Keep the stream near real time when Wi-Fi stalls: discard the oldest frame and enqueue this one.
  AudioFrame discarded;
  xQueueReceive(inputQueue_, &discarded, 0);
  return xQueueSend(inputQueue_, &frame, 0) == pdTRUE;
}

void RobotClient::requestBargeIn() { interruptionPending_ = true; }

void RobotClient::networkTask(void* context) {
  static_cast<RobotClient*>(context)->runNetwork();
}

void RobotClient::socketEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (activeClient_ != nullptr) activeClient_->handleSocketEvent(type, payload, length);
}

void RobotClient::handleSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      connected_ = true;
      startSent_ = false;
      outputActive_ = false;
      Serial.println("Live speech WebSocket connected");
      break;
    case WStype_DISCONNECTED:
      connected_ = false;
      startSent_ = false;
      if (outputActive_) audio_->stopPlayback();
      outputActive_ = false;
      Serial.println("Live speech WebSocket disconnected; reconnecting");
      break;
    case WStype_TEXT: {
      if (payload == nullptr) break;
      char eventText[512];
      const size_t textLength = min(length, sizeof(eventText) - 1);
      memcpy(eventText, payload, textLength);
      eventText[textLength] = '\0';
      if (strstr(eventText, "session_started") != nullptr) {
        Serial.println("Live speech provider session started");
      }
      if (strstr(eventText, "\"type\":\"response_complete\"") != nullptr ||
          strstr(eventText, "\"type\":\"error\"") != nullptr ||
          strstr(eventText, "live_session_failed") != nullptr) {
        if (outputActive_) audio_->stopPlayback();
        outputActive_ = false;
      }
      break;
    }
    case WStype_BIN:
      if (payload == nullptr || length == 0 || length > kMaxSocketAudioBytes || length % 2 != 0) break;
      if (!outputActive_) {
        outputActive_ = audio_->startPlayback();
        if (!outputActive_) break;
      }
      if (!audio_->playPcm(payload, length)) {
        audio_->stopPlayback();
        outputActive_ = false;
      }
      break;
    default:
      break;
  }
}

void RobotClient::runNetwork() {
  WiFi.setAutoReconnect(true);
  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Connecting robot to Wi-Fi");
      if (!connectWifi()) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        continue;
      }
    }
    if (socket_.getUrl().isEmpty()) {
      String authorization = String("Authorization: Bearer ") + ROBOT_DEVICE_TOKEN + "\r\n";
      socket_.setExtraHeaders(authorization.c_str());
      socket_.beginSslWithCA(ROBOT_LIVE_WS_HOST, ROBOT_LIVE_WS_PORT, ROBOT_LIVE_WS_PATH, ROBOT_ROOT_CA_PEM);
    }

    socket_.loop();
    if (connected_ && !startSent_) {
      startSent_ = socket_.sendTXT(kStartMessage);
      if (startSent_) Serial.println("Live PCM16 microphone stream started at 16 kHz mono");
    }
    if (connected_ && interruptionPending_) {
      if (socket_.sendTXT(kInterruptMessage)) interruptionPending_ = false;
    }
    if (connected_) {
      AudioFrame frame;
      if (xQueueReceive(inputQueue_, &frame, pdMS_TO_TICKS(4)) == pdTRUE) {
        socket_.sendBIN(reinterpret_cast<const uint8_t*>(frame.samples), frame.frames * sizeof(int16_t));
      }
    } else {
      AudioFrame discarded;
      xQueueReceive(inputQueue_, &discarded, 0);
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
}
