#include "include/AudioEngine.h"
#include "include/ContinuousVoiceAgent.h"
#include "include/RobotClient.h"

#include <Arduino.h>

namespace {
AudioEngine audio;
RobotClient client;
ContinuousVoiceAgent voiceAgent;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Starting hands-free voice robot");
  if (!audio.begin()) {
    Serial.println("Audio/AEC initialization failed; reboot after checking firmware/core configuration");
    while (true) delay(1000);
  }
  if (!voiceAgent.begin(audio, client)) {
    Serial.println("Voice task or PSRAM buffer initialization failed");
    while (true) delay(1000);
  }
  Serial.println("Voice robot initialized; speech is detected automatically");
}

void loop() {
  delay(1000);  // Listener and network worker run independently from startup.
}
