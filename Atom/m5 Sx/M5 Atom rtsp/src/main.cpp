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
  Serial.println("Starting M5 Atom RTSP server");
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());

  // Configure I2S in PDM mode
  I2SConfig cfg = i2sStream.defaultConfig(RX_MODE);
  cfg.signal_type = PDM;
  cfg.pin_ws   = 22;     // CLK (change to your pin)
  cfg.pin_data = 23;     // DATA (change to your pin)
  cfg.sample_rate = audioInfo.sample_rate;
  cfg.bits_per_sample = audioInfo.bits_per_sample;
  cfg.channels = audioInfo.channels;
  Serial.printf("Configuring I2S (WS:%d, DATA:%d, rate:%dHz)\n", cfg.pin_ws, cfg.pin_data, cfg.sample_rate);
  if (!i2sStream.begin(cfg)) {
    Serial.println("I2S configuration failed");
  } else {
    Serial.println("I2S configured");
  }

  rtspServer.init(RTSPServer::AUDIO_ONLY, 8554, audioInfo.sample_rate);
  Serial.println("RTSP server initialized");
  Serial.printf("RTSP URL: rtsp://%s:8554/mic\n", WiFi.localIP().toString().c_str());
}

void loop() {
  static unsigned long lastLog = 0;
  if (rtspServer.readyToSendAudio()) {
    size_t bytesRead = i2sStream.readBytes((uint8_t*)sampleBuffer, sizeof(sampleBuffer));
    bool sent = false;
    if (bytesRead) {
      rtspServer.sendRTSPAudio(sampleBuffer, bytesRead / sizeof(int16_t));
      sent = true;
    }
    if (millis() - lastLog > 1000) {
      Serial.printf("bytesRead: %u, sent: %s\n", bytesRead, sent ? "yes" : "no");
      lastLog = millis();
    }
  }
  delay(1);
}
