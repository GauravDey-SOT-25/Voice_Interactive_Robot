#pragma once

#include <stdint.h>
#include <stddef.h>

namespace RobotConfig {
constexpr int kMicI2sPort = 0;
constexpr int kSpeakerI2sPort = 1;
constexpr int kMicBclkPin = 21;
constexpr int kMicWsPin = 38;
constexpr int kMicDataPin = 47;
constexpr int kSpeakerBclkPin = 41;
constexpr int kSpeakerWsPin = 42;
constexpr int kSpeakerDataPin = 48;
constexpr uint32_t kMicSampleRate = 16000;
constexpr uint32_t kSpeakerSampleRate = 24000;
constexpr uint32_t kSilenceEndMs = 800;
constexpr uint32_t kMinimumUtteranceMs = 250;
constexpr uint32_t kMaximumUtteranceMs = 15000;
constexpr uint32_t kPreRollMs = 320;
constexpr uint32_t kBargeInConfirmMs = 220;
constexpr float kNoiseFloorTrackingAlpha = 0.05f;
constexpr float kNoiseGateRatio = 2.2f;
constexpr int16_t kNoiseGateMinimumThreshold = 160;
constexpr float kNoiseFloorMaximum = 1000.0f;
constexpr uint32_t kNetworkRetryInitialMs = 1000;
constexpr uint32_t kNetworkRetryMaximumMs = 30000;
constexpr uint8_t kMaximumNetworkAttempts = 5;
constexpr size_t kPreRollFrames = kMicSampleRate * kPreRollMs / 1000;
constexpr size_t kMaximumUtteranceFrames = kMicSampleRate * kMaximumUtteranceMs / 1000;
constexpr size_t kCaptureCapacityFrames = kPreRollFrames + kMaximumUtteranceFrames;
}
