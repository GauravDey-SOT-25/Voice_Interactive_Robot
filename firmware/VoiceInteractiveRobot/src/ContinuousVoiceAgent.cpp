#include "../include/ContinuousVoiceAgent.h"

#include "../include/AudioEngine.h"
#include "../include/RobotClient.h"
#include "../include/RobotConfig.h"

#include <esp_heap_caps.h>

bool ContinuousVoiceAgent::begin(AudioEngine& audio, RobotClient& client) {
  audio_ = &audio;
  client_ = &client;
  frame_ = static_cast<int16_t*>(heap_caps_malloc(1024 * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (frame_ == nullptr || !client.startLive(audio)) return false;
  return xTaskCreatePinnedToCore(listenerTask, "voice_listen", 8192, this, 5, nullptr, 1) == pdPASS;
}

void ContinuousVoiceAgent::listenerTask(void* context) {
  static_cast<ContinuousVoiceAgent*>(context)->listenLoop();
}

void ContinuousVoiceAgent::listenLoop() {
  Serial.println("Always listening: local AEC + VAD, streaming mic PCM to live speech service");
  while (true) {
    size_t frames = 0;
    bool speech = false;
    if (!audio_->readEnhanced(frame_, 1024, &frames, &speech)) {
      Serial.println("Audio front-end read failed; retrying microphone");
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    client_->sendAudio(frame_, frames);
    const uint32_t now = millis();
    if (speech && audio_->isPlaybackActive()) {
      if (!bargeInTriggered_ && bargeInSinceMs_ == 0) bargeInSinceMs_ = now;
      if (!bargeInTriggered_ && now - bargeInSinceMs_ >= RobotConfig::kBargeInConfirmMs) {
        audio_->requestBargeIn();
        client_->requestBargeIn();
        bargeInTriggered_ = true;
        Serial.println("Barge-in detected; interrupting live response");
      }
    } else {
      bargeInSinceMs_ = 0;
      bargeInTriggered_ = false;
    }
    taskYIELD();
  }
}
