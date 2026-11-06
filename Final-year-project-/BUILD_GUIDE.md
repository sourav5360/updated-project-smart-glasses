# Step-by-Step Build Guide: Smart Glasses HUD v2.0 (0.23" HDMI Micro-OLED Prism + Pro Sensors Edition)

This guide covers building the complete **Pro Smart Glasses HUD** featuring the **Raspberry Pi Zero 2 W**, **0.23" HDMI Micro-OLED Optical Prism**, **WM8960 + Bone Conduction Audio**, **Raspberry Pi Camera Module 3 (POV Vision)**, **APDS-9960 Hand Gesture Sensor**, **BNO055/MPU6050 AR Compass IMU**, and **PiSugar 3 / MAX17048 Smart Battery Fuel Gauge** (while remaining 100% compatible with the 0.96" SSD1306 I2C OLED and 1.3" ST7789 SPI IPS displays).

---

## 1. Upgraded Pro Bill of Materials (BOM)

| # | Component | Model / Specification | Interface |
|---|---|---|---|
| 1 | **Compute Board** | **Raspberry Pi Zero 2 W** (Quad-Core 64-bit Cortex-A53, Wi-Fi + BT) | Main Board |
| 2 | **AR Optical Display** | **0.23" HDMI Micro-OLED Optical Prism (640x400, 3000 nits)** + FPC Mini-HDMI Ribbon | **Mini-HDMI** (`/dev/fb0`) |
| 3 | **Audio Codec + Transducer** | **Waveshare WM8960 Audio HAT** + **8Ω 1W Bone Conduction Transducer** | **I2C (`0x1A`) + I2S** |
| 4 | **POV Camera** | **Raspberry Pi Camera Module 3** (or **ZeroCam 8MP**) + Zero CSI Ribbon | **CSI Camera Slot** |
| 5 | **Hand Gesture Sensor** | **APDS-9960 RGB & Gesture Sensor** (Contactless Swipe Left/Right/Up/Down) | **I2C (`0x39`)** |
| 6 | **Head-Tracking & Compass** | **BNO055 9-Axis IMU** (`0x28`) or **MPU6050 6-Axis IMU** (`0x68`) | **I2C (`0x28` / `0x68`)** |
| 7 | **Smart Battery + Fuel Gauge** | **PiSugar 3 (1200mAh)** (`0x57`) or **LiPo + MAX17048 Fuel Gauge** (`0x36`) | **I2C (`0x36` / `0x57`)** |

---

## 2. Complete Wiring & Bus Architecture (Zero Pin Conflicts)

1. **Mini-HDMI Port**: Connect the **0.23" HDMI Micro-OLED Optical Prism** via a flexible FPC Mini-HDMI ribbon cable. Linux automatically exposes this display as `/dev/fb0`, which [`hdmi_prism_hud.c`](main/source/hdmi_prism_hud.c) renders to in 32-bit ARGB color.
2. **CSI Camera Connector**: Connect the **Raspberry Pi Camera Module 3 / ZeroCam** ribbon cable.
3. **Shared I2C1 Bus (`Pin 1 = 3.3V`, `Pin 3 = GPIO 2 SDA`, `Pin 5 = GPIO 3 SCL`, `Pin 6 = GND`)**:
   Every I2C peripheral has a unique hardware address so they can all be wired in parallel to **Pins 1, 3, 5, and 6**:
   - `0x1A` — **WM8960 Audio HAT** (drives the **8Ω 1W Bone Conduction Transducer** + dual MEMS mics)
   - `0x39` — **APDS-9960 Hand Gesture Sensor**
   - `0x28` / `0x68` — **BNO055 / MPU6050 IMU** (AR Compass heading & head-tilt detection)
   - `0x36` / `0x57` — **MAX17048 / PiSugar 3 Battery Fuel Gauge** (Glasses battery %)
   - `0x3C` — **0.96" SSD1306 OLED** *(optional secondary/fallback display)*

---

## 3. Software Installation & Compilation

### Step 1: Enable I2C & SPI and Install Packages
```bash
sudo apt-get update && sudo apt-get upgrade -y
sudo raspi-config   # Interface Options -> Enable I2C & SPI
sudo apt-get install -y build-essential git i2c-tools libi2c-dev \
    bluetooth bluez libbluetooth-dev espeak alsa-utils
```

### Step 2: Verify Connected I2C Peripherals
```bash
sudo i2cdetect -y 1
```

### Step 3: Install WM8960 Audio HAT Driver
```bash
cd "wm8960 drivers"
chmod +x install.sh
sudo ./install.sh
sudo reboot
```

### Step 4: Compile & Launch the Smart Glasses HUD Engine
```bash
cd "main/source"
make
sudo ./raspberry
```

---

## 4. Complete Protocol Reference (v2.0 Pro)

Send newline-terminated strings over **Bluetooth RFCOMM (Channel 1)**, **Wi-Fi TCP (Port 8888)**, or **local `STDIN`**:

| Command | Example | Description |
|---|---|---|
| `SYS\|<HH:MM>\|<Date>\|<Batt%>\|<Weather>` | `SYS\|14:35\|Fri Nov 6\|88\|24C Clear` | Syncs phone clock, date, battery %, and weather |
| `NOTIF\|<App>\|<Title>\|<Body>` | `NOTIF\|WHATSAPP\|Alice\|On my way!` | Displays Notification Card + icon + Bone Conduction TTS |
| `NAV\|<Dir>\|<Dist>\|<Street>\|<ETA>` | `NAV\|LEFT\|150 m\|Turn left on Oak\|4 min` | Displays Turn-by-Turn arrow, distance, street & ETA |
| `MUSIC\|<PLAY/PAUSE>\|<Title>\|<Artist>\|<%>` | `MUSIC\|PLAY\|Starboy\|The Weeknd\|65` | Displays Now Playing HUD & progress bar |
| `CALL\|<RINGING/ENDED>\|<Name>\|<Number>` | `CALL\|RINGING\|Mom\|+1-555-0199` | High-priority Incoming Call overlay + voice alert |
| `CAM\|SNAP\|<filepath>` | `CAM\|SNAP\|/tmp/glasses_pov.jpg` | Captures 720p POV photo on Pi Camera & notifies phone |
| `COMPASS\|<0-359>` | `COMPASS\|270` | Updates AR Compass heading (`270 deg W`) |
| `GLASSES_BATT\|<0-100>` | `GLASSES_BATT\|94` | Updates Glasses battery % readout |
| `GESTURE\|<LEFT/RIGHT/UP/DOWN>` | `GESTURE\|RIGHT` | Triggers contactless hand-swipe action |
| `BMP\|<2048_hex>` | `BMP\|AA55...` | Projects 128x64 monochrome bitmap frame |
| `TTS\|<Speech Text \| ON \| OFF>` | `TTS\|Route recalculated` | Speaks text via WM8960 Bone Conduction Transducer |
| `MIRROR\|<flipX:0/1>\|<flipY:0/1>` | `MIRROR\|1\|0` | Flips display horizontally/vertically for prism optics |
| `MODE\|<DASHBOARD/NOTIF/NAV/MUSIC/CAMERA>` | `MODE\|DASHBOARD` | Switches active HUD view |
