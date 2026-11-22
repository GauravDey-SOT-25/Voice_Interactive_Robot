#include "../include/AudioEngine.h"

#include "../include/RobotConfig.h"

#include <Arduino.h>
#include <driver/i2s_std.h>

namespace {
constexpr size_t kDmaFrames = 256;
}

bool AudioEngine::begin() { return true; }

void AudioEngine::destroyChannel() {
  if (channel_ != nullptr) {
    i2s_chan_handle_t channel = static_cast<i2s_chan_handle_t>(channel_);
    i2s_channel_disable(channel);
    i2s_del_channel(channel);
    channel_ = nullptr;
  }
}

bool AudioEngine::createChannel(bool receive) {
  destroyChannel();
  i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(
      static_cast<i2s_port_t>(RobotConfig::kI2sPort), I2S_ROLE_MASTER);
  channelConfig.dma_desc_num = 6;
  channelConfig.dma_frame_num = kDmaFrames;
  i2s_chan_handle_t channel = nullptr;
  esp_err_t result = i2s_new_channel(&channelConfig, receive ? nullptr : &channel,
                                     receive ? &channel : nullptr);
  if (result != ESP_OK || channel == nullptr) return false;

  i2s_std_config_t config = {};
  config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(receive ? RobotConfig::kMicSampleRate
                                                      : RobotConfig::kSpeakerSampleRate);
  if (receive) {
    config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                           I2S_SLOT_MODE_MONO);
  } else {
    config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                           I2S_SLOT_MODE_MONO);
  }
  config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  config.gpio_cfg.bclk = static_cast<gpio_num_t>(RobotConfig::kBclkPin);
  config.gpio_cfg.ws = static_cast<gpio_num_t>(RobotConfig::kWsPin);
  config.gpio_cfg.dout = receive ? I2S_GPIO_UNUSED : static_cast<gpio_num_t>(RobotConfig::kSpeakerDataPin);
  config.gpio_cfg.din = receive ? static_cast<gpio_num_t>(RobotConfig::kMicDataPin) : I2S_GPIO_UNUSED;
  config.gpio_cfg.invert_flags.mclk_inv = false;
  config.gpio_cfg.invert_flags.bclk_inv = false;
  config.gpio_cfg.invert_flags.ws_inv = false;
  result = i2s_channel_init_std_mode(channel, &config);
  if (result != ESP_OK || i2s_channel_enable(channel) != ESP_OK) {
    i2s_del_channel(channel);
    return false;
  }
  channel_ = channel;
  return true;
}

bool AudioEngine::startCapture() { return createChannel(true); }

bool AudioEngine::readCapture(int16_t* destination, size_t frames, size_t* framesRead) {
  if (channel_ == nullptr || destination == nullptr || framesRead == nullptr) return false;
  int32_t raw[kDmaFrames];
  size_t total = 0;
  while (total < frames) {
    const size_t batch = min(kDmaFrames, frames - total);
    size_t bytesRead = 0;
    esp_err_t result = i2s_channel_read(static_cast<i2s_chan_handle_t>(channel_), raw,
                                        batch * sizeof(int32_t), &bytesRead, 1000);
    if (result != ESP_OK && result != ESP_ERR_TIMEOUT) return false;
    const size_t got = bytesRead / sizeof(int32_t);
    for (size_t i = 0; i < got; ++i) convertInmp441Word(raw[i], &destination[total + i]);
    total += got;
    if (got == 0 && result == ESP_ERR_TIMEOUT) break;
  }
  *framesRead = total;
  return true;
}

bool AudioEngine::stopCapture() {
  if (channel_ == nullptr) return true;
  destroyChannel();
  return true;
}

bool AudioEngine::startPlayback() { return createChannel(false); }

bool AudioEngine::playPcm(const uint8_t* data, size_t bytes) {
  if (channel_ == nullptr || data == nullptr || bytes % sizeof(int16_t) != 0) return false;
  size_t offset = 0;
  while (offset < bytes) {
    size_t written = 0;
    esp_err_t result = i2s_channel_write(static_cast<i2s_chan_handle_t>(channel_), data + offset,
                                         bytes - offset, &written, 1000);
    if (result != ESP_OK || written == 0) return false;
    offset += written;
  }
  return true;
}

bool AudioEngine::stopPlayback() {
  if (channel_ == nullptr) return true;
  destroyChannel();
  return true;
}

void AudioEngine::convertInmp441Word(int32_t word, int16_t* output) {
  // INMP441 emits left-justified 24-bit signed data in the 32-bit I2S slot.
  int32_t sample = word >> 16;
  if (sample > INT16_MAX) sample = INT16_MAX;
  if (sample < INT16_MIN) sample = INT16_MIN;
  *output = static_cast<int16_t>(sample);
}
