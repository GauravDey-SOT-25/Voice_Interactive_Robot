#include "../include/Wav.h"

namespace {
void put16(uint8_t* p, uint16_t value) {
  p[0] = static_cast<uint8_t>(value);
  p[1] = static_cast<uint8_t>(value >> 8);
}
void put32(uint8_t* p, uint32_t value) {
  p[0] = static_cast<uint8_t>(value);
  p[1] = static_cast<uint8_t>(value >> 8);
  p[2] = static_cast<uint8_t>(value >> 16);
  p[3] = static_cast<uint8_t>(value >> 24);
}
}

void writeWavHeader(uint8_t* h, uint32_t pcmBytes, uint32_t sampleRate) {
  h[0] = 'R'; h[1] = 'I'; h[2] = 'F'; h[3] = 'F';
  put32(h + 4, 36 + pcmBytes);
  h[8] = 'W'; h[9] = 'A'; h[10] = 'V'; h[11] = 'E';
  h[12] = 'f'; h[13] = 'm'; h[14] = 't'; h[15] = ' ';
  put32(h + 16, 16); put16(h + 20, 1); put16(h + 22, 1);
  put32(h + 24, sampleRate); put32(h + 28, sampleRate * 2);
  put16(h + 32, 2); put16(h + 34, 16);
  h[36] = 'd'; h[37] = 'a'; h[38] = 't'; h[39] = 'a';
  put32(h + 40, pcmBytes);
}
