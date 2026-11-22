#pragma once

// Copy to Secrets.h (ignored by Git), then fill in local values.
#define ROBOT_WIFI_SSID "your-wifi"
#define ROBOT_WIFI_PASSWORD "your-password"
#define ROBOT_API_URL "https://robot-api.example.com/v1/voice/audio"
#define ROBOT_DEVICE_TOKEN "replace-with-long-random-token"

// Root CA that issued the server certificate, in PEM format including BEGIN/END lines.
// Do not disable TLS validation. Keep this file local if it contains a private CA.
#define ROBOT_ROOT_CA_PEM ""
