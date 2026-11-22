#pragma once

#include <stdint.h>
#include <stddef.h>

namespace RobotConfig {
constexpr int kI2sPort = 0;
constexpr int kBclkPin = 21;
constexpr int kWsPin = 38;
constexpr int kMicDataPin = 47;
constexpr int kSpeakerDataPin = 48;
constexpr int kPushToTalkPin = 0;  // T4-S3 active-low BOOT button
constexpr uint32_t kMicSampleRate = 16000;
constexpr uint32_t kSpeakerSampleRate = 24000;
constexpr uint32_t kMaxRecordingSeconds = 10;
constexpr uint32_t kCapturePollMs = 20;
constexpr size_t kCaptureFrames = kMicSampleRate * kMaxRecordingSeconds;
}
