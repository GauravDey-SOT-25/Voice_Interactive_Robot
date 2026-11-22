#pragma once

#include <stddef.h>
#include <stdint.h>

class AudioEngine {
 public:
  bool begin();
  bool readEnhanced(int16_t* destination, size_t capacity, size_t* framesRead, bool* speechDetected);
  bool startPlayback();
  bool playPcm(const uint8_t* data, size_t bytes);
  bool stopPlayback();
  bool isPlaybackActive() const;
  bool wasBargeInRequested() const;
  void requestBargeIn();
  static void convertInmp441Word(int32_t word, int16_t* output);

 private:
  bool createMicrophoneChannel();
  bool createSpeakerChannel();
  void destroySpeakerChannel();
  void pushPlaybackReference(const int16_t* samples, size_t count);
  void pushPlaybackReferenceFrames(const int16_t* samples, size_t count);
  void popPlaybackReference(int16_t* samples, size_t count);
  void resetPlaybackReference();
  void* microphoneChannel_ = nullptr;
  void* speakerChannel_ = nullptr;
  void* afeHandle_ = nullptr;
  void* afeData_ = nullptr;
  int16_t* afeFeedBuffer_ = nullptr;
  int16_t* microphoneBuffer_ = nullptr;
  int16_t* referenceBuffer_ = nullptr;
  int16_t* referenceRing_ = nullptr;
  volatile uint32_t referenceWrite_ = 0;
  volatile uint32_t referenceRead_ = 0;
  int16_t resampleGroup_[3] = {};
  uint8_t resampleCount_ = 0;
  float noiseFloor_ = 180.0f;
  volatile bool playbackActive_ = false;
  volatile bool bargeInRequested_ = false;
};
