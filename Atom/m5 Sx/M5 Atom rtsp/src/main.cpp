#include <Arduino.h>
#include <WiFi.h>
#include <M5Unified.h>
#include "AudioTools.h"
#include "AudioLibs/RTSPServer.h"

// WiFi credentials
const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Audio configuration: 16 kHz mono, 16-bit
AudioInfo   audioInfo(16000, 1, 16);
I2SStream   i2sStream;
RTSPServer  rtsp(audioInfo, i2sStream);

void setup() {
  M5.begin();
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  I2SConfig cfg = i2sStream.defaultConfig(RX_MODE_PDM);
  cfg.pin_ws   = 22;  // Clock pin
  cfg.pin_data = 23;  // Data pin
  cfg.sample_rate = audioInfo.sample_rate;
  cfg.bits_per_sample = audioInfo.bits_per_sample;
  cfg.channels = audioInfo.channels;
  i2sStream.begin(cfg);

  rtsp.setDefaultTransport(Transport::TCP);
  rtsp.begin("mic");

  Serial.printf("RTSP URL: rtsp://%s/mic\n", WiFi.localIP().toString().c_str());
}

void loop() {
  rtsp.handleClient();
  rtsp.write(i2sStream);
}
