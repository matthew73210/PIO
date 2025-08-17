#include <Arduino.h>
#include <WiFi.h>
#include <M5Unified.h>
#include "AudioTools.h"
#include <ESP32-RTSPServer.h>
#include <esp_log.h>

// ================= USER TOGGLES =================
// 1 = run RTSP on port 554 (some libs advertise :554); 0 = use 8554
#define FORCE_PORT_554   0
// 0 = stream microphone, 1 = stream a 1 kHz test tone (diagnostic)
#define USE_TEST_TONE    1

// ================= Wi-Fi ========================
const char* ssid     = "Burton the 3rd";
const char* password = "Zineb the 1st";

// ================= Audio params =================
static constexpr uint32_t SAMPLE_RATE = 16000;   // Hz
static constexpr uint8_t  CHANNELS    = 1;       // mono
static constexpr uint8_t  BPS         = 16;      // bits per sample
static constexpr size_t   FRAME_MS    = 20;      // RTP frame duration
static constexpr size_t   SAMPLES_PER_FRAME = (SAMPLE_RATE * FRAME_MS) / 1000; // 320 @16k/20ms

// ================= Globals ======================
AudioInfo   audioInfo(SAMPLE_RATE, CHANNELS, BPS);
I2SStream   i2sStream;
RTSPServer  rtspServer;

// ESP32 I2S DMA: buffer_size <= 1024 bytes and > 8
static constexpr size_t READ_SAMPLES = 512; // 512 * 2 bytes = 1024 bytes
int16_t readBuf[READ_SAMPLES];

// Ring buffer to decouple read cadence from send cadence
static constexpr size_t RING_SAMPLES = SAMPLES_PER_FRAME * 24; // ~480ms headroom
int16_t ringBuf[RING_SAMPLES];
size_t  ringHead = 0, ringTail = 0;

static inline size_t ringAvail() {
  return (ringHead + RING_SAMPLES - ringTail) % RING_SAMPLES;
}
static inline size_t ringFree() {
  return RING_SAMPLES - 1 - ringAvail();
}
static inline void ringPush(const int16_t* src, size_t n) {
  for (size_t i=0;i<n;i++) { ringBuf[ringHead] = src[i]; ringHead = (ringHead + 1) % RING_SAMPLES; }
}
static inline void ringPop(int16_t* dst, size_t n) {
  for (size_t i=0;i<n;i++) { dst[i] = ringBuf[ringTail]; ringTail = (ringTail + 1) % RING_SAMPLES; }
}

// Simple level meter (serial diagnostics)
struct Meter { int64_t acc = 0; int16_t peak = 0; size_t n = 0; };
static inline void meterFeed(Meter& m, const int16_t* s, size_t n) {
  for (size_t i=0;i<n;i++) { int16_t v = s[i]; int16_t a = abs(v); m.acc += a; if (a > m.peak) m.peak = a; }
  m.n += n;
}
static inline void meterPrintAndReset(Meter& m) {
  if (m.n == 0) return;
  int32_t avg = (int32_t)(m.acc / (int64_t)m.n);
  Serial.printf("[AUDIO] Level avg=%ld peak=%d (of 32767) over %u samples\n", (long)avg, (int)m.peak, (unsigned)m.n);
  m = Meter{};
}

// ============ OPTIONAL: TEST TONE ===========
#if USE_TEST_TONE
static uint32_t tonePhase = 0;
static inline void genTone(int16_t* dst, size_t n, uint32_t freq = 1000) {
  const uint32_t inc = (uint32_t)((uint64_t)freq * 0xFFFFFFFFull / SAMPLE_RATE);
  for (size_t i=0;i<n;i++) {
    tonePhase += inc;
    float ph = (tonePhase / 4294967296.0f) * 2.0f * 3.1415926535f;
    dst[i] = (int16_t)(sinf(ph) * 12000.0f);
  }
}
#endif

void setup() {
  M5.begin();
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_DEBUG);

  Serial.println("\n===== BOOT =====");
  Serial.printf("[SYS] Chip: %s, Cores:%d, Rev:%d\n", ESP.getChipModel(), ESP.getChipCores(), ESP.getChipRevision());

  Serial.println("[WiFi] Connecting…");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(200); Serial.print("."); }
  WiFi.setSleep(false); // timing stability
  Serial.printf("\n[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());

#if !USE_TEST_TONE
  // ---- I2S PDM IN ----
  I2SConfig cfg = i2sStream.defaultConfig(RX_MODE);
  cfg.signal_type     = PDM;     // PDM microphone
  cfg.pin_bck         = 32;      // CLK on Atom (G32)
  cfg.pin_data        = 26;      // DATA on Atom (G26)
  cfg.pin_ws          = -1;      // not used for PDM
  cfg.sample_rate     = SAMPLE_RATE;
  cfg.bits_per_sample = BPS;
  cfg.channels        = CHANNELS;
  cfg.buffer_size     = 1024;    // bytes (driver limit)
  cfg.buffer_count    = 8;       // >=2
  cfg.use_apll        = true;    // steadier clock

  Serial.printf("[I2S] Config: PDM CLK=%d DATA=%d %luHz %ubit mono, buf=%u x %uB\n",
                cfg.pin_bck, cfg.pin_data,
                (unsigned long)cfg.sample_rate, (unsigned)cfg.bits_per_sample,
                (unsigned)cfg.buffer_count, (unsigned)cfg.buffer_size);

  if (!i2sStream.begin(cfg)) {
    Serial.println("[I2S] begin() FAILED — stopping");
    while (true) { delay(1000); }
  }
  Serial.println("[I2S] Ready");
#else
  Serial.println("[I2S] Test tone mode: microphone disabled");
#endif

  // ---- RTSP ----
  const int rtspPort = FORCE_PORT_554 ? 554 : 8554;
  if (!rtspServer.init(RTSPServer::AUDIO_ONLY, rtspPort, SAMPLE_RATE)) {
    Serial.println("[RTSP] init() FAILED — stopping");
    while (true) { delay(1000); }
  }
  Serial.printf("[RTSP] Ready on port %d\n", rtspPort);
  Serial.printf("[RTSP] URL: rtsp://%s:%d/mic\n", WiFi.localIP().toString().c_str(), rtspPort);
  Serial.printf("[RTSP] VLC hint: vlc --rtsp-tcp rtsp://%s:%d/mic\n",
                WiFi.localIP().toString().c_str(), rtspPort);
  Serial.println("===== SETUP COMPLETE =====\n");
}

void loop() {
  // 1) Acquire audio into ring (1024-byte reads or generated tone)
#if USE_TEST_TONE
  size_t got = READ_SAMPLES;
  genTone(readBuf, got);
#else
  size_t bytesRead = i2sStream.readBytes((uint8_t*)readBuf, sizeof(readBuf));
  size_t got = bytesRead / sizeof(int16_t);
#endif

  if (got > 0) {
    size_t freeS = ringFree();
    if (got > freeS) {
      Serial.printf("[RING] Overflow: dropping %u samples (free %u)\n", (unsigned)(got - freeS), (unsigned)freeS);
      got = freeS;
    }
    if (got) ringPush(readBuf, got);
  }

  // 2) RTSP send fixed 20ms frames with prebuffer and pacing
  static bool     client  = false;
  static bool     started = false;
  static uint64_t next_us = 0;
  static uint64_t frames  = 0;
  static int16_t  frame[SAMPLES_PER_FRAME];
  static Meter    meter;

  if (rtspServer.readyToSendAudio()) {
    if (!client) {
      client  = true;
      started = false;   // require prebuffer again
      frames  = 0;
      Serial.println("[RTSP] Client CONNECTED");
    }

    // Prebuffer ~500 ms before first packet
    const size_t PRE_FRAMES = 25;
    size_t avail = ringAvail();
    if (!started) {
      if (avail >= PRE_FRAMES * SAMPLES_PER_FRAME) {
        started = true;
        next_us = (uint64_t)micros(); // start cadence now
        Serial.printf("[RTSP] Prebuffer ok (%u samples). Starting stream.\n", (unsigned)avail);
      } else {
        static uint32_t lastRpt = 0;
        if (millis() - lastRpt > 500) {
          Serial.printf("[RTSP] Prebuffering… avail=%u / need=%u\n",
                        (unsigned)avail, (unsigned)(PRE_FRAMES * SAMPLES_PER_FRAME));
          lastRpt = millis();
        }
        delay(2);
        return;
      }
    }

    // Pace: exactly one frame every 20 ms
    uint64_t now = (uint64_t)micros();
    if (now + 2000ULL < next_us) { delay(1); return; }   // early, yield
    if (now < next_us) { while ((uint64_t)micros() < next_us) { /* spin */ } }

    // Pop one frame; if short, send silence
    if (ringAvail() >= SAMPLES_PER_FRAME) {
      ringPop(frame, SAMPLES_PER_FRAME);
    } else {
      memset(frame, 0, sizeof(frame));
      Serial.println("[RTSP] Underflow — sent silence");
    }

    // Level meter (prints ~1/sec)
    meterFeed(meter, frame, SAMPLES_PER_FRAME);
    static uint32_t lastMs = 0;
    if (millis() - lastMs > 1000) {
      meterPrintAndReset(meter);
      Serial.printf("[DBG] frames=%llu, ring_avail=%u\n", (unsigned long long)frames, (unsigned)ringAvail());
      lastMs = millis();
    }

    // **** Send: library expects int16_t* and sample count ****
    rtspServer.sendRTSPAudio(frame, SAMPLES_PER_FRAME);

    frames++;
    next_us += (uint64_t)FRAME_MS * 1000ULL;

    // small drift trim
    int64_t drift = (int64_t)((int64_t)micros() - (int64_t)next_us);
    if (drift > 5000)       { next_us += 2000; Serial.println("[CLK] drift +, easing forward 2ms"); }
    else if (drift < -5000) { next_us -= 2000; Serial.println("[CLK] drift -, easing back 2ms"); }

  } else {
    if (client) {
      client  = false;
      started = false;
      Serial.println("[RTSP] Client DISCONNECTED");
    }
    // heartbeat every 2s so you know loop is alive even without client
    static uint32_t lastBeat = 0;
    if (millis() - lastBeat > 2000) {
      Serial.printf("[HB] ring_avail=%u\n", (unsigned)ringAvail());
      lastBeat = millis();
    }
    delay(2);
  }
}