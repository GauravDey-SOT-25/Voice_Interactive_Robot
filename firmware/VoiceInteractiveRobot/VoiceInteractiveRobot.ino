#include "include/AudioEngine.h"
#include "include/RobotClient.h"
#include "include/RobotConfig.h"

#include <Arduino.h>

namespace {
AudioEngine audio;
RobotClient client;
int16_t* capture = nullptr;
size_t capturedFrames = 0;
bool recording = false;

void beginRecording() {
  if (capture == nullptr) {
    capture = static_cast<int16_t*>(ps_malloc(RobotConfig::kCaptureFrames * sizeof(int16_t)));
  }
  if (capture == nullptr || !audio.startCapture()) {
    Serial.println("Audio capture could not start (PSRAM/I2S unavailable)");
    return;
  }
  capturedFrames = 0;
  recording = true;
  Serial.println("Recording; release BOOT to send (10 second maximum)");
}

void finishRecording() {
  audio.stopCapture();
  recording = false;
  if (capturedFrames == 0) {
    Serial.println("No audio captured");
    return;
  }
  Serial.printf("Uploading %u audio frames\n", static_cast<unsigned>(capturedFrames));
  if (!client.processVoice(capture, capturedFrames, audio)) {
    Serial.println("Voice turn failed; check Wi-Fi, HTTPS/API configuration, and logs");
  }
  Serial.println("Ready; hold BOOT to talk");
}
}

void setup() {
  Serial.begin(115200);
  pinMode(RobotConfig::kPushToTalkPin, INPUT_PULLUP);
  audio.begin();
  Serial.println("T4-S3 voice robot ready; connect to Wi-Fi and hold BOOT to record");
  if (!client.connectWifi()) Serial.println("Wi-Fi not connected; will retry after push-to-talk");
}

void loop() {
  if (!recording && digitalRead(RobotConfig::kPushToTalkPin) == LOW) {
    beginRecording();
    delay(50);
    return;
  }
  if (!recording) { delay(20); return; }

  const size_t blockFrames = RobotConfig::kMicSampleRate * RobotConfig::kCapturePollMs / 1000;
  size_t framesRead = 0;
  if (!audio.readCapture(capture + capturedFrames, blockFrames, &framesRead)) {
    audio.stopCapture();
    recording = false;
    Serial.println("I2S capture error");
    return;
  }
  capturedFrames += framesRead;
  const bool heldTooLong = capturedFrames >= RobotConfig::kCaptureFrames;
  const bool released = digitalRead(RobotConfig::kPushToTalkPin) == HIGH;
  if (released || heldTooLong) finishRecording();
}
