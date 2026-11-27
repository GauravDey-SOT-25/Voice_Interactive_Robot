#pragma once

// Copy to Secrets.h (ignored by Git), then fill in local values.
struct RobotWifiCredential {
  const char* ssid;
  const char* password;
};

static const RobotWifiCredential ROBOT_WIFI_NETWORKS[] = {
    {"your-wifi", "your-password"},
    // Add more entries; WiFiMulti selects the strongest configured network.
};
#define ROBOT_API_URL "https://robot-api.example.com/v1/voice/audio"
#define ROBOT_LIVE_WS_HOST "robot-api.example.com"
#define ROBOT_LIVE_WS_PORT 443
#define ROBOT_LIVE_WS_PATH "/v1/voice/live?provider=auto"
#define ROBOT_DEVICE_TOKEN "replace-with-long-random-token"

// Root CA that issued the server certificate, in PEM format including BEGIN/END lines.
// Do not disable TLS validation. Keep this file local if it contains a private CA.
#define ROBOT_ROOT_CA_PEM ""
