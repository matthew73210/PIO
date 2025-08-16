#include <Arduino.h>
#include <WiFi.h>
#include "AudioTools.h"              // I2S + streaming helpers
#include "AudioLibs/RTSPServer.h"    // RTSP over TCP

const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

AudioInfo   audioInfo(16000, 1, 16); // 16‑kHz mono, 16‑bit PCM
I2SStream   i2sStream;
RTSPServer  rtsp(audioInfo, i2sStream);  // serves the I2S stream

void setup() {
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

  rtsp.setDefaultTransport(Transport::TCP);  // RTP-over-TCP
  rtsp.begin("mic");                         // rtsp://<ip>/mic
  Serial.printf("RTSP URL: rtsp://%s/mic\n", WiFi.localIP().toString().c_str());
}

void loop() {
  rtsp.handleClient();   // manage RTSP connections
  rtsp.write(i2sStream); // push audio data to clients
}
