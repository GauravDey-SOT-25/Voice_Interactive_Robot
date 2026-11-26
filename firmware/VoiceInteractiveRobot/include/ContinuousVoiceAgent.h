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
  static void listenerTask(void* context);
  void listenLoop();

  AudioEngine* audio_ = nullptr;
  RobotClient* client_ = nullptr;
  int16_t* frame_ = nullptr;
  uint32_t bargeInSinceMs_ = 0;
  bool bargeInTriggered_ = false;
};
