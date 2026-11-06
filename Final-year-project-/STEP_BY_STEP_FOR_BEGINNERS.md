# Super Simple Guide: How to Build & Set Up Your Pro Smart Glasses (Step-by-Step for Beginners)

Think of your **Pro Smart Glasses** like **6 Lego pieces** that snap into one tiny brain:
1. **The Brain** (**Raspberry Pi Zero 2 W** — a tiny, super-fast computer on your glasses).
2. **The See-Through Prism Screen** (**0.23" HDMI Micro-OLED Optical Prism** — plugs into the Mini-HDMI port and floats a glowing color screen right in front of your eye, with no separate magnifying glass needed!).
3. **Private Bone Conduction Sound** (**WM8960 Audio HAT + Bone Conduction Transducer** — vibrates sound softly through your temple bone so only you can hear calls and messages).
4. **First-Person Camera** (**Pi Camera Module 3 / ZeroCam** — plugs into the camera ribbon slot so your glasses can take photos and videos).
5. **Hand-Wave & Compass Sensors** (**APDS-9960 Gesture Sensor + BNO055/MPU6050 IMU** — wave your hand in the air to flip screens and see a live compass at the top of your view).
6. **Smart Battery** (**PiSugar 3** or **MAX17048 Fuel Gauge** — powers the glasses and shows the exact Glasses Battery % on the screen).

---

## PART 1: Upload This Project to Your GitHub Account (In 1 Minute)

1. Open your browser and go to: **https://github.com/new**
2. In the **Repository name** box, type: `Smart-Glasses-HUD-v2`
3. Scroll down and click the green **Create repository** button.
4. Click the blue link that says **"uploading an existing file"**.
5. Open **File Explorer** on your computer and go to:
   `C:\Users\sharish\.gemini\antigravity\scratch\Final-year-project-`
6. Drag all the files from that folder into the browser window and click **Commit changes**!

---

## PART 2: Plug the Pieces Together (While Unplugged from Power!)

Because your **0.23" HDMI Micro-OLED Prism** and **Camera** have their own special ports, plugging everything in is super easy:

### 1. Plug in the 0.23" HDMI Micro-OLED Optical Prism
- Plug the thin **Mini-HDMI ribbon cable** from the **0.23" Micro-OLED Prism** directly into the **Mini-HDMI port** on the Raspberry Pi Zero 2 W.
- Clip the tiny prism block onto the front of your glasses frame in front of your right eye. *(No separate magnifying glass or mirror needed!)*

### 2. Plug in the POV Camera (Pi Camera Module 3 / ZeroCam)
- Gently lift the plastic clip on the **CSI Camera slot** at the edge of the Raspberry Pi Zero 2 W, slide the flat camera ribbon cable in, and push the clip down.

### 3. Plug in the Sound Board & Bone Conduction Speaker
- Press the **WM8960 Audio HAT** onto the Raspberry Pi's pins.
- Connect the 2 wires of your **8Ω 1W Bone Conduction Transducer** into the speaker screw terminal on the WM8960 board, and rest the flat pad inside the temple arm of the glasses behind your ear.

### 4. Plug in the Hand-Wave Gesture, Compass & Battery Sensors (Same 4 Wires!)
All three smart sensors (**APDS-9960 Gesture**, **BNO055/MPU6050 Compass**, and **MAX17048/PiSugar Battery**) share the **exact same 4 pins** on the Raspberry Pi:

```text
   [ SENSOR PINS ]                   [ RASPBERRY PI ZERO 2 W ]
   ---------------                   -------------------------
   1. VCC (or VIN)  -------------->  Pin 1  (Top-left corner pin = 3.3V Power)
   2. SDA           -------------->  Pin 3  (Second pin down on the left = Data)
   3. SCL           -------------->  Pin 5  (Third pin down on the left = Clock)
   4. GND           -------------->  Pin 6  (Third pin down on the right = Ground)
```
*(Note: If you still want to test with your old 4-pin SSD1306 OLED or 1.3" ST7789 screen, our software supports them at the exact same time too!)*

---

## PART 3: Set Up the Raspberry Pi Zero 2 W (Copy & Paste One by One)

Turn on your Raspberry Pi and open the **Terminal** (the black box icon).

### Step 1: Turn on the Sensor & Screen Connectors (I2C & SPI)
Type this and press **Enter**:
```bash
sudo raspi-config
```
1. Highlight **Interface Options** -> press **Enter**.
2. Choose **I2C** -> select **Yes**.
3. Choose **SPI** -> select **Yes**.
4. Select **Finish**.

### Step 2: Install the Helper Tools
Copy and paste this whole line and press **Enter**:
```bash
sudo apt-get update && sudo apt-get install -y build-essential git i2c-tools libi2c-dev bluetooth bluez libbluetooth-dev espeak alsa-utils
```

### Step 3: Check That Your Sensors Are Plugged in Right
Type this and press **Enter**:
```bash
sudo i2cdetect -y 1
```
Look at the grid of numbers that appears:
- `1a` = **WM8960 Bone Conduction Audio HAT**
- `39` = **APDS-9960 Hand Gesture Sensor**
- `28` (or `68`) = **BNO055 / MPU6050 AR Compass IMU**
- `36` (or `57`) = **MAX17048 / PiSugar 3 Battery Fuel Gauge**
- `3c` = **SSD1306 OLED** (if also attached)

### Step 4: Build and Start the Smart Glasses Program!
Inside your project folder, run:
```bash
cd "main/source"
make
sudo ./raspberry
```
**Look into your 0.23" HDMI Micro-OLED Prism!** You will see the **Full-Color AR HUD** floating in front of your eye with:
- A glowing **AR Compass Bar** (`< 045 deg NE >`) at the top
- **Dual Battery Meters** (`P:100% G:96%` for both your Phone and your Glasses)
- Large **Digital Clock, Date, and Weather**

---

## PART 4: Test Every Feature Right Now (Type into the Terminal!)

While `sudo ./raspberry` is running, type any of these lines into the terminal and press **Enter**:

1. **Test a WhatsApp Message (Glows Emerald Green on the Prism!):**
   ```text
   NOTIF|WHATSAPP|Best Friend|Hey! Your new HDMI Micro-OLED Prism looks futuristic!
   ```
2. **Test a Turn-by-Turn Google Maps Arrow (Glows Navigation Blue!):**
   ```text
   NAV|LEFT|150 m|Turn left on Main St|4 mins
   ```
3. **Snap a First-Person Photo with the Pi Camera:**
   ```text
   CAM|SNAP|/tmp/glasses_pov.jpg
   ```
4. **Rotate the AR Compass Bar to 270 Degrees (West):**
   ```text
   COMPASS|270
   ```
5. **Simulate Waving Your Hand Right in Front of the Gesture Sensor:**
   ```text
   GESTURE|RIGHT
   ```
6. **Make the Bone Conduction Speaker Talk:**
   ```text
   TTS|Photo captured. Heading West at 270 degrees.
   ```
7. **Flip the Prism Image (if your prism optics need mirroring):**
   ```text
   MIRROR|1|0
   ```

---

## PART 5: Connect Your Android Phone

1. **Pair Bluetooth**:
   Open a new terminal tab on the Pi, run `sudo bluetoothctl`, and type:
   ```text
   power on
   discoverable on
   pairable on
   agent on
   ```
   Then pair with **`raspberrypi`** in your phone's Bluetooth settings.
2. **Install the Companion App**:
   - Open [`android-app/`](android-app/) in **Android Studio**, plug in your phone via USB, and click **Run (Play)**.
   - Tap **"Grant Notif Access"**, connect via **Bluetooth** or **Wi-Fi**, and use the new **"Snap POV Photo"** and **"Swipe << / >>"** buttons!
