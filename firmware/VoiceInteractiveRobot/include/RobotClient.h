#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include <WiFiMulti.h>
#include <WebSocketsClient.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

class AudioEngine;

class RobotClient {
 public:
  bool connectWifi();
  bool startLive(AudioEngine& audio);
  bool sendAudio(const int16_t* samples, size_t frameCount);
  void requestBargeIn();
 private:
  struct AudioFrame {
    uint16_t frames;
    int16_t samples[1024];
  };
  static void networkTask(void* context);
  static void socketEvent(WStype_t type, uint8_t* payload, size_t length);
  void runNetwork();
  void handleSocketEvent(WStype_t type, uint8_t* payload, size_t length);

  AudioEngine* audio_ = nullptr;
  WiFiMulti wifiMulti_;
  QueueHandle_t inputQueue_ = nullptr;
  WebSocketsClient socket_;
  volatile bool connected_ = false;
  volatile bool startSent_ = false;
  volatile bool interruptionPending_ = false;
  bool outputActive_ = false;
  uint32_t lastWifiQualityCheckMs_ = 0;
  uint32_t lastWifiReselectMs_ = 0;
  static RobotClient* activeClient_;
};
