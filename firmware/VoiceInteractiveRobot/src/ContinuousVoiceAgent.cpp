#include "../include/ContinuousVoiceAgent.h"

#include "../include/AudioEngine.h"
#include "../include/RobotClient.h"
#include "../include/RobotConfig.h"

#include <esp_heap_caps.h>

bool ContinuousVoiceAgent::begin(AudioEngine& audio, RobotClient& client) {
  audio_ = &audio;
  client_ = &client;
  constexpr size_t frameCapacity = 1024;
  const size_t captureBytes = RobotConfig::kCaptureCapacityFrames * sizeof(int16_t);
  for (int i = 0; i < 2; ++i) {
    utterances_[i] = static_cast<int16_t*>(heap_caps_malloc(captureBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!utterances_[i]) return false;
  }
  frame_ = static_cast<int16_t*>(heap_caps_malloc(frameCapacity * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  preRoll_ = static_cast<int16_t*>(heap_caps_malloc(RobotConfig::kPreRollFrames * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  queue_ = xQueueCreate(2, sizeof(Utterance));
  if (!frame_ || !preRoll_ || !queue_) return false;
  if (xTaskCreatePinnedToCore(listenerTask, "voice_listen", 8192, this, 5, nullptr, 1) != pdPASS) return false;
  if (xTaskCreatePinnedToCore(processorTask, "voice_process", 12288, this, 3, nullptr, 0) != pdPASS) return false;
  return true;
}

void ContinuousVoiceAgent::listenerTask(void* context) {
  static_cast<ContinuousVoiceAgent*>(context)->listenLoop();
}

void ContinuousVoiceAgent::processorTask(void* context) {
  static_cast<ContinuousVoiceAgent*>(context)->processLoop();
}

int ContinuousVoiceAgent::findFreeSlot() const {
  for (int i = 0; i < 2; ++i) if (!slotBusy_[i]) return i;
  return -1;
}

void ContinuousVoiceAgent::listenLoop() {
  Serial.println("Always listening (AFE VAD + AEC); no button or wake word required");
  while (true) {
    size_t frames = 0;
    bool speech = false;
    if (!audio_->readEnhanced(frame_, 1024, &frames, &speech)) {
      Serial.println("Audio front-end read failed; retrying microphone");
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    const uint32_t now = millis();
    if (speech) {
      lastSpeechAtMs_ = now;
      if (activeSlot_ < 0) {
        if (droppingPhrase_) {
          // A phrase begun while both PSRAM slots were busy is dropped in full.
        } else if (audio_->isPlaybackActive()) {
          if (bargeInSinceMs_ == 0) bargeInSinceMs_ = now;
          if (now - bargeInSinceMs_ >= RobotConfig::kBargeInConfirmMs) audio_->requestBargeIn();
        } else {
          bargeInSinceMs_ = 0;
          beginUtterance();
        }
      }
    } else {
      if (activeSlot_ < 0) bargeInSinceMs_ = 0;
      if (droppingPhrase_ && now - lastSpeechAtMs_ >= RobotConfig::kSilenceEndMs) droppingPhrase_ = false;
    }

    if (activeSlot_ >= 0) {
      appendUtterance(frame_, frames);  // Keep trailing silence so the backend receives natural phrase endings.
      if (speech) speechFrames_ += frames;
      if (speechFrames_ >= RobotConfig::kMaximumUtteranceFrames ||
          (!speech && now - lastSpeechAtMs_ >= RobotConfig::kSilenceEndMs)) finishUtterance();
    } else {
      appendPreRoll(frame_, frames);
    }
    taskYIELD();
  }
}

void ContinuousVoiceAgent::appendPreRoll(const int16_t* samples, size_t frames) {
  for (size_t i = 0; i < frames; ++i) {
    preRoll_[preRollWrite_] = samples[i];
    preRollWrite_ = (preRollWrite_ + 1) % RobotConfig::kPreRollFrames;
    if (preRollCount_ < RobotConfig::kPreRollFrames) ++preRollCount_;
  }
}

void ContinuousVoiceAgent::beginUtterance() {
  activeSlot_ = findFreeSlot();
  if (activeSlot_ < 0) {
    droppingPhrase_ = true;
    Serial.println("Utterance buffer full; current phrase will be dropped");
    return;
  }
  slotBusy_[activeSlot_] = true;
  utteranceFrames_ = 0;
  speechFrames_ = 0;
  const size_t start = (preRollWrite_ + RobotConfig::kPreRollFrames - preRollCount_) % RobotConfig::kPreRollFrames;
  for (size_t i = 0; i < preRollCount_; ++i) {
    const size_t index = (start + i) % RobotConfig::kPreRollFrames;
    utterances_[activeSlot_][utteranceFrames_++] = preRoll_[index];
  }
  lastSpeechAtMs_ = millis();
  bargeInSinceMs_ = 0;
  Serial.println("Speech detected; capturing phrase");
}

void ContinuousVoiceAgent::appendUtterance(const int16_t* samples, size_t frames) {
  if (activeSlot_ < 0) return;
  const size_t room = RobotConfig::kCaptureCapacityFrames - utteranceFrames_;
  const size_t count = min(room, frames);
  memcpy(utterances_[activeSlot_] + utteranceFrames_, samples, count * sizeof(int16_t));
  utteranceFrames_ += count;
}

void ContinuousVoiceAgent::finishUtterance() {
  if (activeSlot_ < 0) return;
  const size_t minimumFrames = RobotConfig::kMicSampleRate * RobotConfig::kMinimumUtteranceMs / 1000;
  if (speechFrames_ < minimumFrames) {
    slotBusy_[activeSlot_] = false;
    activeSlot_ = -1;
    utteranceFrames_ = 0;
    speechFrames_ = 0;
    return;
  }
  slotFrameCounts_[activeSlot_] = utteranceFrames_;
  Utterance utterance = {static_cast<uint8_t>(activeSlot_)};
  if (xQueueSend(queue_, &utterance, 0) != pdTRUE) {
    Serial.println("Utterance queue full; phrase discarded");
    slotFrameCounts_[activeSlot_] = 0;
    slotBusy_[activeSlot_] = false;
  } else {
    Serial.printf("Finalized phrase (%u ms)\n", static_cast<unsigned>(utteranceFrames_ * 1000 / RobotConfig::kMicSampleRate));
  }
  activeSlot_ = -1;
  utteranceFrames_ = 0;
  speechFrames_ = 0;
}

void ContinuousVoiceAgent::processLoop() {
  uint32_t retryDelayMs = RobotConfig::kNetworkRetryInitialMs;
  while (true) {
    Utterance utterance;
    if (xQueueReceive(queue_, &utterance, portMAX_DELAY) != pdTRUE) continue;
    const uint8_t slot = utterance.slot;
    size_t frameCount = slotFrameCounts_[slot];
    uint8_t attempts = 0;
    while (frameCount > 0) {
      ++attempts;
      if (client_->processVoice(utterances_[slot], frameCount, *audio_)) {
        retryDelayMs = RobotConfig::kNetworkRetryInitialMs;
        break;
      }
      if (audio_->wasBargeInRequested()) {
        retryDelayMs = RobotConfig::kNetworkRetryInitialMs;
        break;
      }
      if (attempts >= RobotConfig::kMaximumNetworkAttempts) {
        Serial.println("Voice request retry limit reached; phrase dropped, listening continues");
        break;
      }
      Serial.printf("Voice request failed; retrying in %u ms while listening continues\n", static_cast<unsigned>(retryDelayMs));
      vTaskDelay(pdMS_TO_TICKS(retryDelayMs));
      retryDelayMs = min(retryDelayMs * 2, RobotConfig::kNetworkRetryMaximumMs);
      // Retry this same utterance once connectivity/API configuration may have recovered.
    }
    slotFrameCounts_[slot] = 0;
    slotBusy_[slot] = false;
  }
}
