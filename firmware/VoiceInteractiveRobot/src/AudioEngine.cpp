#include "../include/AudioEngine.h"

#include "../include/RobotConfig.h"

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <esp_afe_config.h>
#include <esp_afe_sr_iface.h>
#include <esp_afe_sr_models.h>
#include <esp_aec.h>
#include <esp_heap_caps.h>

namespace {
constexpr size_t kReferenceFrames = 8192;
constexpr size_t kDmaFrames = 256;
}

bool AudioEngine::begin() {
  afe_config_t* config = afe_config_init("MR", nullptr, AFE_TYPE_FD, AFE_MODE_LOW_COST);
  if (config == nullptr) return false;
  config->aec_init = true;
  config->aec_mode = AEC_MODE_FD_LOW_COST;
  config->aec_filter_length = 4;
  config->vad_init = true;
  config->vad_model_name = nullptr;  // Built-in WebRTC VAD; no wake-word model is loaded.
  config->vad_min_speech_ms = 128;
  config->vad_min_noise_ms = 128;
  config->vad_delay_ms = 128;
  config->ns_init = false;  // No NS model partition is bundled; a lightweight adaptive gate follows AFE output.
  const esp_afe_sr_iface_t* iface = esp_afe_handle_from_config(config);
  if (iface == nullptr) { afe_config_free(config); return false; }
  afeHandle_ = const_cast<esp_afe_sr_iface_t*>(iface);
  afeData_ = iface->create_from_config(config);
  afe_config_free(config);
  if (afeData_ == nullptr) return false;

  const int chunk = iface->get_feed_chunksize(static_cast<esp_afe_sr_data_t*>(afeData_));
  const int channels = iface->get_feed_channel_num(static_cast<esp_afe_sr_data_t*>(afeData_));
  if (chunk <= 0 || channels != 2) return false;
  afeFeedBuffer_ = static_cast<int16_t*>(heap_caps_malloc(chunk * channels * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  microphoneBuffer_ = static_cast<int16_t*>(heap_caps_malloc(chunk * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  referenceBuffer_ = static_cast<int16_t*>(heap_caps_malloc(chunk * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  referenceRing_ = static_cast<int16_t*>(heap_caps_calloc(kReferenceFrames, sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!afeFeedBuffer_ || !microphoneBuffer_ || !referenceBuffer_ || !referenceRing_) return false;
  if (!createMicrophoneChannel()) return false;
  Serial.printf("AFE ready: %d samples/frame, %d feed channels (AEC + WebRTC VAD)\n", chunk, channels);
  return true;
}

bool AudioEngine::createMicrophoneChannel() {
  i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(static_cast<i2s_port_t>(RobotConfig::kMicI2sPort), I2S_ROLE_MASTER);
  channelConfig.dma_desc_num = 8;
  channelConfig.dma_frame_num = kDmaFrames;
  i2s_chan_handle_t channel = nullptr;
  if (i2s_new_channel(&channelConfig, nullptr, &channel) != ESP_OK || channel == nullptr) return false;
  i2s_std_config_t config = {};
  config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RobotConfig::kMicSampleRate);
  config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
  config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  config.gpio_cfg.bclk = static_cast<gpio_num_t>(RobotConfig::kMicBclkPin);
  config.gpio_cfg.ws = static_cast<gpio_num_t>(RobotConfig::kMicWsPin);
  config.gpio_cfg.dout = I2S_GPIO_UNUSED;
  config.gpio_cfg.din = static_cast<gpio_num_t>(RobotConfig::kMicDataPin);
  config.gpio_cfg.invert_flags = {};
  if (i2s_channel_init_std_mode(channel, &config) != ESP_OK || i2s_channel_enable(channel) != ESP_OK) {
    i2s_del_channel(channel);
    return false;
  }
  microphoneChannel_ = channel;
  return true;
}

bool AudioEngine::createSpeakerChannel() {
  i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(static_cast<i2s_port_t>(RobotConfig::kSpeakerI2sPort), I2S_ROLE_MASTER);
  channelConfig.dma_desc_num = 8;
  channelConfig.dma_frame_num = kDmaFrames;
  i2s_chan_handle_t channel = nullptr;
  if (i2s_new_channel(&channelConfig, &channel, nullptr) != ESP_OK || channel == nullptr) return false;
  i2s_std_config_t config = {};
  config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RobotConfig::kSpeakerSampleRate);
  config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  config.gpio_cfg.bclk = static_cast<gpio_num_t>(RobotConfig::kSpeakerBclkPin);
  config.gpio_cfg.ws = static_cast<gpio_num_t>(RobotConfig::kSpeakerWsPin);
  config.gpio_cfg.dout = static_cast<gpio_num_t>(RobotConfig::kSpeakerDataPin);
  config.gpio_cfg.din = I2S_GPIO_UNUSED;
  config.gpio_cfg.invert_flags = {};
  if (i2s_channel_init_std_mode(channel, &config) != ESP_OK || i2s_channel_enable(channel) != ESP_OK) {
    i2s_del_channel(channel);
    return false;
  }
  speakerChannel_ = channel;
  return true;
}

void AudioEngine::destroySpeakerChannel() {
  if (speakerChannel_ != nullptr) {
    i2s_chan_handle_t channel = static_cast<i2s_chan_handle_t>(speakerChannel_);
    i2s_channel_disable(channel);
    i2s_del_channel(channel);
    speakerChannel_ = nullptr;
  }
}

bool AudioEngine::readEnhanced(int16_t* destination, size_t capacity, size_t* framesRead, bool* speechDetected) {
  if (!microphoneChannel_ || !afeHandle_ || !afeData_ || !destination || !framesRead || !speechDetected) return false;
  const auto* iface = static_cast<const esp_afe_sr_iface_t*>(afeHandle_);
  auto* afe = static_cast<esp_afe_sr_data_t*>(afeData_);
  const int chunk = iface->get_feed_chunksize(afe);
  if (chunk <= 0 || capacity < static_cast<size_t>(chunk)) return false;
  int32_t raw[kDmaFrames];
  if (static_cast<size_t>(chunk) > kDmaFrames) return false;
  size_t bytesRead = 0;
  const esp_err_t readResult = i2s_channel_read(static_cast<i2s_chan_handle_t>(microphoneChannel_), raw, chunk * sizeof(int32_t), &bytesRead, 1000);
  if ((readResult != ESP_OK && readResult != ESP_ERR_TIMEOUT) || bytesRead != static_cast<size_t>(chunk) * sizeof(int32_t)) return false;
  for (int i = 0; i < chunk; ++i) {
    convertInmp441Word(raw[i], &microphoneBuffer_[i]);
    popPlaybackReference(&referenceBuffer_[i], 1);
    afeFeedBuffer_[2 * i] = microphoneBuffer_[i];
    afeFeedBuffer_[2 * i + 1] = referenceBuffer_[i];
  }
  if (iface->feed(afe, afeFeedBuffer_) < 0) return false;
  afe_fetch_result_t* result = iface->fetch_with_delay(afe, pdMS_TO_TICKS(100));
  if (result == nullptr || result->ret_value < 0 || result->data == nullptr) return false;
  const size_t outFrames = static_cast<size_t>(result->data_size) / sizeof(int16_t);
  if (outFrames > capacity) return false;
  memcpy(destination, result->data, outFrames * sizeof(int16_t));
  *framesRead = outFrames;
  *speechDetected = result->vad_state == AFE_VAD_SPEECH;
  if (!*speechDetected && outFrames > 0) {
    uint64_t sum = 0;
    for (size_t i = 0; i < outFrames; ++i) sum += abs(static_cast<int>(destination[i]));
    const float mean = static_cast<float>(sum) / outFrames;
    noiseFloor_ += RobotConfig::kNoiseFloorTrackingAlpha * (mean - noiseFloor_);
    noiseFloor_ = min(noiseFloor_, RobotConfig::kNoiseFloorMaximum);
  }
  const int threshold = max(static_cast<int>(RobotConfig::kNoiseGateMinimumThreshold), static_cast<int>(noiseFloor_ * RobotConfig::kNoiseGateRatio));
  for (size_t i = 0; i < outFrames; ++i) {
    const int sample = destination[i];
    const int magnitude = abs(sample);
    if (magnitude < threshold) destination[i] = static_cast<int16_t>(sample * magnitude / threshold);
  }
  return true;
}

bool AudioEngine::startPlayback() {
  if (speakerChannel_ != nullptr) return false;
  resetPlaybackReference();
  bargeInRequested_ = false;
  playbackActive_ = createSpeakerChannel();
  return playbackActive_;
}

bool AudioEngine::playPcm(const uint8_t* data, size_t bytes) {
  if (speakerChannel_ == nullptr || data == nullptr || bytes % sizeof(int16_t) != 0) return false;
  size_t offset = 0;
  while (offset < bytes) {
    if (bargeInRequested_) return false;
    const size_t chunkBytes = min(static_cast<size_t>(1024), bytes - offset) & ~static_cast<size_t>(1);
    size_t written = 0;
    const esp_err_t result = i2s_channel_write(static_cast<i2s_chan_handle_t>(speakerChannel_), data + offset, chunkBytes, &written, 100);
    if (result != ESP_OK || written == 0) return false;
    pushPlaybackReference(reinterpret_cast<const int16_t*>(data + offset), written / sizeof(int16_t));
    offset += written;
  }
  return true;
}

bool AudioEngine::stopPlayback() {
  playbackActive_ = false;
  destroySpeakerChannel();
  return true;
}

bool AudioEngine::isPlaybackActive() const { return playbackActive_; }
bool AudioEngine::wasBargeInRequested() const { return bargeInRequested_; }
void AudioEngine::requestBargeIn() { bargeInRequested_ = true; }

void AudioEngine::pushPlaybackReference(const int16_t* samples, size_t count) {
  // Convert the 24 kHz speaker reference to the 16 kHz AFE reference clock (3:2 linear decimation).
  for (size_t i = 0; i < count; ++i) {
    resampleGroup_[resampleCount_++] = samples[i];
    if (resampleCount_ == 3) {
      const int16_t out[2] = {
        static_cast<int16_t>((static_cast<int32_t>(resampleGroup_[0]) + resampleGroup_[1]) / 2),
        static_cast<int16_t>((static_cast<int32_t>(resampleGroup_[1]) + resampleGroup_[2]) / 2)
      };
      pushPlaybackReferenceFrames(out, 2);
      resampleCount_ = 0;
    }
  }
}

void AudioEngine::pushPlaybackReferenceFrames(const int16_t* samples, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const uint32_t write = referenceWrite_;
    if (write - referenceRead_ < kReferenceFrames) {
      referenceRing_[write % kReferenceFrames] = samples[i];
      referenceWrite_ = write + 1;
    }
  }
}

void AudioEngine::popPlaybackReference(int16_t* samples, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (referenceRead_ == referenceWrite_) samples[i] = 0;
    else samples[i] = referenceRing_[(referenceRead_++) % kReferenceFrames];
  }
}

void AudioEngine::resetPlaybackReference() {
  referenceRead_ = 0;
  referenceWrite_ = 0;
  resampleCount_ = 0;
}

void AudioEngine::convertInmp441Word(int32_t word, int16_t* output) {
  int32_t sample = word >> 16;
  if (sample > INT16_MAX) sample = INT16_MAX;
  if (sample < INT16_MIN) sample = INT16_MIN;
  *output = static_cast<int16_t>(sample);
}
