#include <Arduino.h>
#include <WiFi.h>

// Simple RTSP-like server on port 8554
WiFiServer rtspServer(8554);

void setup() {
  Serial.begin(115200);
  Serial.println("Booting...");

  // Start listening for RTSP clients
  rtspServer.begin();
  Serial.println("RTSP server started on port 8554");
}

void loop() {
  WiFiClient client = rtspServer.available();
  if (client) {
    Serial.println("Client connected");

    // Echo any received data back to the client
    while (client.connected()) {
      if (client.available()) {
        client.write(client.read());
      }
      delay(1);
    }

    Serial.println("Client disconnected");
    client.stop();
  }
}

