# Smart Glasses HUD (Raspberry Pi Zero W + SSD1306 OLED + WM8960 Audio + Android)

Smart Glasses are wearable minicomputer glasses powered by a **Raspberry Pi Zero W**, an **SSD1306 128x64 I2C OLED display**, and a **WM8960 Audio HAT** that project real-time phone notifications, turn-by-turn navigation, media controls, live status dashboards, dithered 128x64 images, and voice TTS audio directly into the wearer's line of sight.

> **Full Step-by-Step Hardware & Software Build Guide**: See [BUILD_GUIDE.md](BUILD_GUIDE.md) or follow the step-by-step instructions below.

---

## Features

1. **Multi-Screen Graphical HUD Engine (`main/source/raspberry.c`, `hud_icons.h`)**:
   - **Persistent Top Status Bar**: Displays active Bluetooth/Wi-Fi icons, time, TTS speaker status, and a dynamic phone battery gauge.
   - **Live Dashboard (`MODE|DASHBOARD`)**: Large size-2 digital clock, date, weather, unread notification preview, and mini media player bar.
   - **All-App Notification Cards (`NOTIF|...`)**: Displays 16x16 app icons (WhatsApp, SMS, Email, Call, Bell), inverted title banner, wrapped message body, and a 10-message history buffer (`NEXT` / `PREV`).
   - **Turn-by-Turn Navigation (`NAV|...`)**: Renders 28x28 directional maneuver arrows (`LEFT`, `RIGHT`, `STRAIGHT`, `SLIGHT_LEFT`, `SLIGHT_RIGHT`, `UTURN`, `ARRIVED`), large distance text, street name, and bottom ETA banner.
   - **Now Playing Media HUD (`MUSIC|...`)**: Track title, artist, play/pause status, and visual progress bar.
   - **Incoming Call Overlay (`CALL|...`)**: High-priority caller ID screen with handset icon and automatic voice announcement.
   - **128x64 Bitmap & Photo Projection (`BMP|<2048_hex>`)**: Projects Floyd-Steinberg dithered 1-bit monochrome images, QR codes, or screen captures directly to the OLED.
2. **10x Faster OLED Driver & Smart Glasses Optics Controls (`ssd1306_i2c.c`, `ssd1306_i2c.h`)**:
   - Uses **32-byte burst I2C transfers** (~30+ FPS) instead of byte-by-byte writes.
   - Includes hardware **Prism Mirror Flip (`MIRROR|1|0`)** to compensate for the 45° combiner lens reflection.
3. **WM8960 Audio HAT Voice & TTS Integration**:
   - Asynchronous text-to-speech (`espeak` + `aplay`) announces incoming calls, messages, or custom `TTS|<text>` packets through the WM8960 speaker without blocking the OLED HUD.
4. **Dual Transport (Bluetooth RFCOMM Channel 1 + Wi-Fi TCP Port 8888)**:
   - Listens simultaneously on Bluetooth RFCOMM and Wi-Fi TCP for seamless phone and laptop connectivity.
5. **Full Android Companion App Source Code (`android-app/`)**:
   - Automatically intercepts phone notifications (WhatsApp, SMS, Gmail, Google Maps Turn-by-Turn, Spotify/Music, Phone Calls) and dithers phone photos to 128x64 monochrome frames.

---

## Step-by-Step Build Guide

### Step 1: Hardware Components (Bill of Materials)
1. **Raspberry Pi Zero W** (with MicroSD card, 8GB+)
2. **0.96" SSD1306 128x64 I2C OLED Display** (4 pins: `VCC`, `GND`, `SCL`, `SDA`)
3. **Waveshare WM8960 Audio HAT** + Speaker & Microphone *(for voice/TTS announcements)*
4. **Biconvex Magnifying Lens** ($f \approx 35\text{–}50\text{ mm}$) + **45° Transparent Combiner Glass/Acrylic**
5. **3.7V LiPo Battery + 5V Power Bank Boost Module**
6. **Eyeglass Frame & Connecting Wires**

### Step 2: Hardware Wiring (Raspberry Pi Zero W to SSD1306 OLED)

| SSD1306 OLED Pin | Raspberry Pi Pin Name | Physical Board Pin # |
|---|---|---|
| **VCC / VIN** | `3.3V` | **Pin 1** |
| **GND** | `GND` | **Pin 6** |
| **SDA** | `GPIO 2 (SDA1)` | **Pin 3** |
| **SCL** | `GPIO 3 (SCL1)` | **Pin 5** |

*(Note: Both the SSD1306 OLED at address `0x3C` and the WM8960 Audio HAT at address `0x1A` share the I2C1 bus on Pins 3 & 5 without conflict.)*

### Step 3: Configure Raspberry Pi OS & Enable I2C
```bash
sudo apt-get update && sudo apt-get upgrade -y
sudo raspi-config
# Go to Interface Options -> I2C -> Enable -> Yes
```
Install required packages:
```bash
sudo apt-get install -y build-essential git i2c-tools libi2c-dev \
    bluetooth bluez libbluetooth-dev espeak alsa-utils
```
Verify the OLED is detected at `0x3c`:
```bash
sudo i2cdetect -y 1
```

### Step 4: Install WM8960 Audio HAT Drivers
```bash
cd "wm8960 drivers"
chmod +x install.sh
sudo ./install.sh
sudo reboot
```

### Step 5: Compile & Run the Smart Glasses HUD Server
```bash
cd "main/source"
make
sudo ./raspberry
```
*(Or compile directly with GCC: `sudo gcc -O2 raspberry.c ssd1306_i2c.c -lwiringPi -lbluetooth -lpthread -o raspberry`)*

### Step 6: Pair Bluetooth & Install the Android Companion App
1. On the Raspberry Pi, pair your phone via `sudo bluetoothctl` (`power on`, `discoverable on`, `pairable on`).
2. Open the [`android-app/`](android-app/) directory in **Android Studio**, build and install the app onto your Android phone (or use the legacy APK in `apk file/`).
3. In the app, tap **Grant Notif Access**, connect via **Bluetooth** or **Wi-Fi (Port 8888)**, and toggle **Prism Flip X** if your combiner mirror flips the image horizontally.

---

## Protocol Reference

Send newline-terminated strings over **Bluetooth RFCOMM (Channel 1)**, **Wi-Fi TCP (Port 8888)**, or **local `STDIN`**:

| Command Format | Example | Description |
|---|---|---|
| `SYS\|<HH:MM>\|<Date>\|<Batt%>\|<Weather>` | `SYS\|14:30\|Fri Nov 6\|88\|24C Clear` | Updates status bar & Home Dashboard |
| `NOTIF\|<App>\|<Title>\|<Body>` | `NOTIF\|WHATSAPP\|Rahul\|Meet at 5 PM` | Shows Notification Card + icon + optional TTS |
| `NAV\|<Dir>\|<Dist>\|<Street>\|<ETA>` | `NAV\|LEFT\|150 m\|Turn left on Oak\|4 min` | Shows 28x28 turn arrow + distance + street + ETA |
| `MUSIC\|<PLAY/PAUSE>\|<Title>\|<Artist>\|<%>` | `MUSIC\|PLAY\|Believer\|Imagine Dragons\|45` | Shows Now Playing screen + progress bar |
| `CALL\|<RINGING/ENDED>\|<Name>\|<Number>` | `CALL\|RINGING\|Mom\|+91 9876543210` | Shows Incoming Call overlay + voice alert |
| `BMP\|<2048_hex_chars>` | `BMP\|FFFF0000...` | Renders full 128x64 1-bit monochrome image |
| `TTS\|<Speech Text \| ON \| OFF>` | `TTS\|Battery is at 88 percent` | Speaks text aloud via WM8960 Audio HAT |
| `MIRROR\|<flipX:0/1>\|<flipY:0/1>` | `MIRROR\|1\|0` | Flips OLED for optical combiner prism |
| `BRIGHTNESS\|<0-255>` | `BRIGHTNESS\|200` | Adjusts OLED contrast |
| `MODE\|<DASHBOARD/NOTIF/NAV/MUSIC>` | `MODE\|DASHBOARD` | Switches active HUD view |
| `NEXT` / `PREV` / `clear` | `NEXT` | Cycles notification history or clears screen |
