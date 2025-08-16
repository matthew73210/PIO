# M5 Atom RTSP Microphone

Streams audio from an M5 Atom with the SPM1423 PDM microphone over RTSP using TCP transport.

## Wiring
- `3V3` ↔ `3V3`
- `GND` ↔ `GND`
- `CLK` ↔ `GPIO22`
- `DATA` ↔ `GPIO23`

## Build & Upload
```bash
pio run --target upload
```

## Monitor
```bash
pio device monitor
```

## Play the Stream
On another machine:
```bash
ffplay rtsp://<board-ip>/mic
# or
vlc rtsp://<board-ip>/mic
```
