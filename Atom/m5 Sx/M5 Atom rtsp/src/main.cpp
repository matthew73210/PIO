#include <Arduino.h>
#include <WiFi.h>
#include <M5Unified.h>               // M5Stack unified device support
#include "AudioTools.h"             // I2S handling
#include <ESP32-RTSPServer.h>        // Updated RTSP server library

const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

AudioInfo   audioInfo(16000, 1, 16); // 16‑kHz mono, 16‑bit PCM
I2SStream   i2sStream;
RTSPServer  rtspServer;              // RTSP server instance
const size_t bufferSamples = 512;
int16_t sampleBuffer[bufferSamples];

void setup() {
  M5.begin();
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(100);

  // Configure I2S in PDM mode
  I2SConfig cfg = i2sStream.defaultConfig(RX_MODE_PDM);
  cfg.pin_ws   = 22;     // CLK (change to your pin)
  cfg.pin_data = 23;     // DATA (change to your pin)
  cfg.sample_rate = audioInfo.sample_rate;
  cfg.bits_per_sample = audioInfo.bits_per_sample;
  cfg.channels = audioInfo.channels;
  i2sStream.begin(cfg);

  rtspServer.init(RTSPServer::AUDIO_ONLY, 8554, audioInfo.sample_rate);
  Serial.printf("RTSP URL: rtsp://%s:8554/mic\n", WiFi.localIP().toString().c_str());
}

void loop() {
  if (rtspServer.readyToSendAudio()) {
    size_t bytesRead = i2sStream.read((uint8_t*)sampleBuffer, sizeof(sampleBuffer));
    if (bytesRead) {
      rtspServer.sendRTSPAudio(sampleBuffer, bytesRead / sizeof(int16_t));
    }
  }
  delay(1);
}
