#ifndef SMART_SENSORS_H_
#define SMART_SENSORS_H_

#include <stdint.h>

/*
 * Smart Glasses Pro Hardware Peripherals Driver:
 *  1. MAX17048 (0x36) / PiSugar 3 (0x57/0x75) I2C Battery Fuel Gauge -> Glasses Battery %
 *  2. MPU6050 (0x68) / BNO055 (0x28) I2C IMU -> AR Compass Heading (0..359 deg) & Head-Tilt Wake
 *  3. APDS-9960 (0x39) I2C Gesture Sensor -> Contactless Hand Swipes (LEFT, RIGHT, UP, DOWN)
 *  4. Raspberry Pi Camera Module 3 / ZeroCam (CSI) -> First-Person POV Photo & Video Capture
 */

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_LEFT,
    GESTURE_RIGHT,
    GESTURE_UP,
    GESTURE_DOWN
} HandGesture;

typedef struct {
    int has_fuel_gauge;
    int has_imu;
    int has_gesture;
    int glasses_battery_pct;   // 0..100% from MAX17048 / PiSugar I2C fuel gauge
    int compass_heading_deg;   // 0..359 degrees
    char compass_cardinal[8];  // "N", "NE", "E", "SE", "S", "SW", "W", "NW"
    float pitch_deg;           // Head pitch angle for auto-wake/sleep
    float roll_deg;            // Head roll angle
    int head_worn_active;      // 1 = worn on head, 0 = laid flat / idle
    char last_photo_path[128]; // Path to latest POV camera snapshot
    int photo_count;
} SmartSensorsData;

// Initialize all I2C sensors on /dev/i2c-1 (non-blocking; gracefully handles missing sensors)
void sensors_init(SmartSensorsData *data);

// Poll I2C fuel gauge & IMU orientation (call periodically)
void sensors_update(SmartSensorsData *data);

// Poll APDS-9960 I2C gesture sensor for a hand swipe event
HandGesture sensors_poll_gesture(SmartSensorsData *data);

// Convert 0..359 heading degrees into cardinal string ("N", "NE", etc.)
const char *sensors_heading_to_cardinal(int heading_deg);

// Trigger an asynchronous POV photo capture using Raspberry Pi Camera (rpicam-still / libcamera-still)
int camera_capture_pov_async(SmartSensorsData *data, const char *filename_out);

#endif /* SMART_SENSORS_H_ */
