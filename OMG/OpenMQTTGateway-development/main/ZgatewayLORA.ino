/*  
  OpenMQTTGateway  - ESP8266 or Arduino program for home automation 

   Act as a wifi or ethernet gateway between your 433mhz/infrared IR/BLE signal  and a MQTT broker 
   Send and receiving command by MQTT
 
  This gateway enables to:
 - receive MQTT data from a topic and send LORA signal corresponding to the received MQTT data
 - publish MQTT data to a different topic related to received LORA signal

    Copyright: (c)Florian ROBERT
  
    This file is part of OpenMQTTGateway.
    
    OpenMQTTGateway is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenMQTTGateway is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "User_config.h"

#ifdef ZgatewayLORA

#  include <LoRa.h>
#  include <SPI.h>
#  include <Wire.h>
#  include <Adafruit_GFX.h>
#  include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
//#  include "images.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

 //packet packet_counter
 int packet_counter = 0;

void setupLORA() {
  //Reset OLED Display via softare
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);
    //initialize OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
     for(;;); // Don't proceed, loop forever
  }
 // display.init();
 // display.flipScreenVertically();  
 // display.setFont(ArialMT_Plain_10);
  Log.notice(F("Display Initialization Successful" CR));
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print(Gateway_Name);
  display.display();
  delay(20);
  Log.notice(F("display Load Done" CR));
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DI0);

  if (!LoRa.begin(LORA_BAND)) {
    Log.error(F("ZgatewayLORA setup failed!" CR));
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0,0);
    display.print("LoRa Setup Failed");
    display.display();
    while (1)
    ;

  }
  LoRa.receive();
  Log.notice(F("LORA_SCK: %d" CR), LORA_SCK);
  Log.notice(F("LORA_MISO: %d" CR), LORA_MISO);
  Log.notice(F("LORA_MOSI: %d" CR), LORA_MOSI);
  Log.notice(F("LORA_SS: %d" CR), LORA_SS);
  Log.notice(F("LORA_RST: %d" CR), LORA_RST);
  Log.notice(F("LORA_DI0: %d" CR), LORA_DI0);
  Log.trace(F("ZgatewayLORA setup done" CR));
}

void LORAtoMQTT() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    StaticJsonDocument<JSON_MSG_BUFFER> jsonBuffer;
    JsonObject LORAdata = jsonBuffer.to<JsonObject>();
    Log.trace(F("Rcv. LORA" CR));
#  ifdef ESP32
    String taskMessage = "LORA Task running on core ";
    taskMessage = taskMessage + xPortGetCoreID();
    //trc(taskMessage);
#  endif
    String packet;
    packet = "";
    for (int i = 0; i < packetSize; i++) {
      packet += (char)LoRa.read();
    }
    LORAdata["rssi"] = (int)LoRa.packetRssi();
    LORAdata["snr"] = (float)LoRa.packetSnr();
    LORAdata["pferror"] = (float)LoRa.packetFrequencyError();
    LORAdata["packetSize"] = (int)packetSize;
    LORAdata["message"] = (char*)packet.c_str();
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("LORA Receiver");
    display.setCursor(0,20);
    display.setTextSize(1);
    display.print("LoRa packet Received");
    display.setCursor(0,30);
    display.print("Packets Published:");
    display.setCursor(50,45);
    display.print(packet_counter);      
    display.display();

    packet_counter++;

    pub(subjectLORAtoMQTT, LORAdata);
    if (repeatLORAwMQTT) {
      Log.trace(F("Pub LORA for rpt" CR));
      pub(subjectMQTTtoLORA, LORAdata);
    }
  }
}

#  ifdef jsonReceiving
void MQTTtoLORA(char* topicOri, JsonObject& LORAdata) { // json object decoding
  if (cmpToMainTopic(topicOri, subjectMQTTtoLORA)) {
    Log.trace(F("MQTTtoLORA json" CR));
    const char* message = LORAdata["message"];
    int txPower = LORAdata["txpower"] | LORA_TX_POWER;
    int spreadingFactor = LORAdata["spreadingfactor"] | LORA_SPREADING_FACTOR;
    long int frequency = LORAdata["frequency "] | LORA_BAND;
    long int signalBandwidth = LORAdata["signalbandwidth"] | LORA_SIGNAL_BANDWIDTH;
    int codingRateDenominator = LORAdata["codingrate"] | LORA_CODING_RATE;
    int preambleLength = LORAdata["preamblelength"] | LORA_PREAMBLE_LENGTH;
    byte syncWord = LORAdata["syncword"] | LORA_SYNC_WORD;
    bool Crc = LORAdata["enablecrc"] | DEFAULT_CRC;
    if (message) {
      LoRa.setTxPower(txPower);
      LoRa.setFrequency(frequency);
      LoRa.setSpreadingFactor(spreadingFactor);
      LoRa.setSignalBandwidth(signalBandwidth);
      LoRa.setCodingRate4(codingRateDenominator);
      LoRa.setPreambleLength(preambleLength);
      LoRa.setSyncWord(syncWord);
      if (Crc)
        LoRa.enableCrc();
      LoRa.beginPacket();
      LoRa.print(message);
      LoRa.endPacket();
      Log.trace(F("MQTTtoLORA OK" CR));
      pub(subjectGTWLORAtoMQTT, LORAdata); // we acknowledge the sending by publishing the value to an acknowledgement topic, for the moment even if it is a signal repetition we acknowledge also
    } else {
      Log.error(F("MQTTtoLORA Fail json" CR));
    }
  }
}
#  endif
#  ifdef simpleReceiving
void MQTTtoLORA(char* topicOri, char* LORAdata) { // json object decoding
  if (cmpToMainTopic(topicOri, subjectMQTTtoLORA)) {
    LoRa.beginPacket();
    LoRa.print(LORAdata);
    LoRa.endPacket();
    Log.notice(F("MQTTtoLORA OK" CR));
    pub(subjectGTWLORAtoMQTT, LORAdata); // we acknowledge the sending by publishing the value to an acknowledgement topic, for the moment even if it is a signal repetition we acknowledge also
  }
}
#  endif
#endif