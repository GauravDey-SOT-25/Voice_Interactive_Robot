#pragma once

#include <stddef.h>
#include <stdint.h>

class AudioEngine {
 public:
  bool begin();
  bool startCapture();
  bool readCapture(int16_t* destination, size_t frames, size_t* framesRead);
  bool stopCapture();
  bool startPlayback();
  bool playPcm(const uint8_t* data, size_t bytes);
  bool stopPlayback();
  static void convertInmp441Word(int32_t word, int16_t* output);

 private:
  bool createChannel(bool receive);
  void destroyChannel();
  void* channel_ = nullptr;
};
