#pragma once

#include <stddef.h>
#include <stdint.h>

void writeWavHeader(uint8_t* output, uint32_t pcmBytes, uint32_t sampleRate);
