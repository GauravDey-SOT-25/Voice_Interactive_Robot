#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

class AudioEngine;

class RobotClient {
 public:
  bool connectWifi();
  bool processVoice(const int16_t* samples, size_t frameCount, AudioEngine& audio);
 private:
  String conversationId_;
};
