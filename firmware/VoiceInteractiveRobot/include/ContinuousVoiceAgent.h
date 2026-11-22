#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

class AudioEngine;
class RobotClient;

class ContinuousVoiceAgent {
 public:
  bool begin(AudioEngine& audio, RobotClient& client);

 private:
  struct Utterance { uint8_t slot; };
  static void listenerTask(void* context);
  static void processorTask(void* context);
  void listenLoop();
  void processLoop();
  int findFreeSlot() const;
  void appendPreRoll(const int16_t* samples, size_t frames);
  void beginUtterance();
  void appendUtterance(const int16_t* samples, size_t frames);
  void finishUtterance();

  AudioEngine* audio_ = nullptr;
  RobotClient* client_ = nullptr;
  QueueHandle_t queue_ = nullptr;
  int16_t* utterances_[2] = {nullptr, nullptr};
  size_t slotFrameCounts_[2] = {0, 0};
  volatile bool slotBusy_[2] = {false, false};
  bool droppingPhrase_ = false;
  int16_t* frame_ = nullptr;
  int16_t* preRoll_ = nullptr;
  size_t preRollWrite_ = 0;
  size_t preRollCount_ = 0;
  size_t utteranceFrames_ = 0;
  size_t speechFrames_ = 0;
  uint32_t lastSpeechAtMs_ = 0;
  uint32_t bargeInSinceMs_ = 0;
  int activeSlot_ = -1;
};
