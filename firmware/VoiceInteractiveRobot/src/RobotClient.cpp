#include "../include/RobotClient.h"

#include "../include/AudioEngine.h"
#include "../include/RobotSecrets.h"
#include "../include/Wav.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace {
constexpr char kBoundary[] = "----T4S3VoiceBoundary42";
constexpr size_t kHeaderBytes = 44;
constexpr size_t kMaxResponseBytes = 2 * 1024 * 1024;

class MultipartStream final : public Stream {
 public:
  MultipartStream(const uint8_t* wav, size_t wavBytes, const String& boundary)
      : wav_(wav), wavBytes_(wavBytes),
        prefix_("--" + boundary + "\r\nContent-Disposition: form-data; name=\"device_id\"\r\n\r\nt4-s3\r\n"),
        suffix_("\r\n--" + boundary + "--\r\n") {}
  void setConversationId(const String& id, const String& boundary) {
    const String filePart = "\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"voice.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
    const String conversationPart = "\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"conversation_id\"\r\n\r\n" + id;
    prefix_ = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"device_id\"\r\n\r\nt4-s3" + conversationPart + filePart;
  }
  int available() override { return static_cast<int>(prefix_.length() + wavBytes_ + suffix_.length() - position_); }
  int read() override { uint8_t byte; return readBytes(&byte, 1) == 1 ? byte : -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t write(uint8_t) override { return 0; }
  size_t readBytes(char* buffer, size_t length) override {
    return readBytes(reinterpret_cast<uint8_t*>(buffer), length);
  }
  size_t readBytes(uint8_t* buffer, size_t length) {
    size_t count = 0;
    while (count < length && position_ < totalLength()) {
      if (position_ < prefix_.length()) buffer[count++] = prefix_[position_++];
      else if (position_ < prefix_.length() + wavBytes_) buffer[count++] = wav_[position_++ - prefix_.length()];
      else buffer[count++] = suffix_[position_++ - prefix_.length() - wavBytes_];
    }
    return count;
  }
  size_t totalLength() const { return prefix_.length() + wavBytes_ + suffix_.length(); }
 private:
  const uint8_t* wav_;
  size_t wavBytes_;
  String prefix_;
  String suffix_;
  size_t position_ = 0;
};
}

bool RobotClient::connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ROBOT_WIFI_SSID, ROBOT_WIFI_PASSWORD);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

bool RobotClient::processVoice(const int16_t* samples, size_t frameCount, AudioEngine& audio) {
  if (WiFi.status() != WL_CONNECTED && !connectWifi()) return false;
  const size_t pcmBytes = frameCount * sizeof(int16_t);
  const size_t wavBytes = kHeaderBytes + pcmBytes;
  uint8_t* wav = static_cast<uint8_t*>(ps_malloc(wavBytes));
  if (wav == nullptr) return false;
  writeWavHeader(wav, pcmBytes, 16000);
  memcpy(wav + kHeaderBytes, samples, pcmBytes);

  WiFiClientSecure tls;
  tls.setCACert(ROBOT_ROOT_CA_PEM);
  HTTPClient http;
  http.setTimeout(60000);
  const String url = ROBOT_API_URL;
  if (!http.begin(tls, url)) { free(wav); return false; }
  const String boundary = kBoundary;
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Authorization", "Bearer " ROBOT_DEVICE_TOKEN);
  static const char* responseHeaders[] = {"X-Audio-Format", "X-Sample-Rate", "X-Channels", "X-Conversation-Id"};
  http.collectHeaders(responseHeaders, 4);

  MultipartStream body(wav, wavBytes, boundary);
  if (!conversationId_.isEmpty()) body.setConversationId(conversationId_, boundary);
  const int status = http.sendRequest("POST", &body, body.totalLength());
  free(wav);
  if (status != HTTP_CODE_OK) {
    Serial.printf("Voice API failed: HTTP %d\n", status);
    http.end();
    return false;
  }
  conversationId_ = http.header("X-Conversation-Id");
  if (http.header("X-Audio-Format") != "s16le" || http.header("X-Sample-Rate") != "24000" || http.header("X-Channels") != "1") {
    Serial.println("Voice API returned unsupported audio format");
    http.end();
    return false;
  }
  const int length = http.getSize();
  if (length <= 0 || static_cast<size_t>(length) > kMaxResponseBytes || length % 2 != 0) {
    Serial.println("Voice API returned invalid PCM length");
    http.end();
    return false;
  }
  Serial.printf("Voice turn received (conversation %s)\n", http.header("X-Conversation-Id").c_str());
  if (!audio.startPlayback()) { http.end(); return false; }
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[2048];
  size_t remaining = static_cast<size_t>(length);
  bool ok = true;
  while (remaining > 0) {
    const size_t requested = min(sizeof(buffer), remaining);
    const int available = stream->available();
    if (available <= 0) {
      if (!stream->connected()) { ok = false; break; }
      delay(1);
      continue;
    }
    size_t readLimit = min(requested, static_cast<size_t>(available));
    readLimit &= ~static_cast<size_t>(1);  // preserve complete s16le samples
    if (readLimit == 0) { delay(1); continue; }
    const size_t count = stream->readBytes(buffer, readLimit);
    if (count == 0) { ok = false; break; }
    if (!audio.playPcm(buffer, count)) {
      ok = false;
      if (audio.wasBargeInRequested()) Serial.println("Barge-in accepted; stopping current response audio");
      break;
    }
    remaining -= count;
  }
  audio.stopPlayback();
  http.end();
  return ok;
}
