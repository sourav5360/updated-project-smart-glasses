/*
 * Implementation of Smart Glasses Pro Hardware Peripherals:
 *  - MAX17048 / PiSugar 3 I2C LiPo Fuel Gauge (0x36 / 0x57)
 *  - MPU6050 / BNO055 I2C 6/9-Axis IMU (0x68 / 0x28)
 *  - APDS-9960 I2C Hand Gesture Sensor (0x39)
 *  - Raspberry Pi Camera Module 3 / ZeroCam POV Capture
 */

#include "smart_sensors.h"
#include <wiringPiI2C.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#define I2C_ADDR_MAX17048  0x36
#define I2C_ADDR_PISUGAR3  0x57
#define I2C_ADDR_MPU6050   0x68
#define I2C_ADDR_BNO055    0x28
#define I2C_ADDR_APDS9960  0x39

static int fd_fuel = -1;
static int fd_imu  = -1;
static int fd_gest = -1;
static int is_pisugar = 0;
static int is_bno055  = 0;

const char *sensors_heading_to_cardinal(int heading_deg)
{
    static const char *dirs[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    int norm = ((heading_deg % 360) + 360) % 360;
    int idx = ((norm + 22) / 45) % 8;
    return dirs[idx];
}

void sensors_init(SmartSensorsData *data)
{
    if (!data) return;
    memset(data, 0, sizeof(*data));
    data->glasses_battery_pct = 96; // Default fallback if gauge is not connected yet
    data->compass_heading_deg = 45;
    strncpy(data->compass_cardinal, "NE", sizeof(data->compass_cardinal) - 1);
    data->head_worn_active = 1;

    // 1. Probe MAX17048 (0x36) or PiSugar 3 (0x57) Battery Fuel Gauge
    fd_fuel = wiringPiI2CSetup(I2C_ADDR_MAX17048);
    if (fd_fuel >= 0 && wiringPiI2CReadReg8(fd_fuel, 0x04) >= 0) {
        data->has_fuel_gauge = 1;
        is_pisugar = 0;
        printf("[Sensors] MAX17048 Battery Fuel Gauge detected at 0x36\n");
    } else {
        fd_fuel = wiringPiI2CSetup(I2C_ADDR_PISUGAR3);
        if (fd_fuel >= 0 && wiringPiI2CReadReg8(fd_fuel, 0x2A) >= 0) {
            data->has_fuel_gauge = 1;
            is_pisugar = 1;
            printf("[Sensors] PiSugar 3 Smart Battery detected at 0x57\n");
        }
    }

    // 2. Probe BNO055 (0x28) or MPU6050 (0x68) IMU for AR Compass & Head Tilt
    fd_imu = wiringPiI2CSetup(I2C_ADDR_BNO055);
    if (fd_imu >= 0 && wiringPiI2CReadReg8(fd_imu, 0x00) == 0xA0) {
        data->has_imu = 1;
        is_bno055 = 1;
        wiringPiI2CWriteReg8(fd_imu, 0x3D, 0x0C); // NDOF 9-axis fusion mode
        printf("[Sensors] BNO055 9-Axis IMU Compass detected at 0x28\n");
    } else {
        fd_imu = wiringPiI2CSetup(I2C_ADDR_MPU6050);
        if (fd_imu >= 0 && wiringPiI2CReadReg8(fd_imu, 0x75) >= 0) {
            data->has_imu = 1;
            is_bno055 = 0;
            wiringPiI2CWriteReg8(fd_imu, 0x6B, 0x00); // Wake up MPU6050
            printf("[Sensors] MPU6050 6-Axis IMU detected at 0x68\n");
        }
    }

    // 3. Probe APDS-9960 (0x39) Hand Gesture Sensor
    fd_gest = wiringPiI2CSetup(I2C_ADDR_APDS9960);
    if (fd_gest >= 0) {
        int id = wiringPiI2CReadReg8(fd_gest, 0x92);
        if (id == 0xAB || id == 0x9C) {
            data->has_gesture = 1;
            wiringPiI2CWriteReg8(fd_gest, 0x80, 0x45); // Enable Power + Proximity + Gesture
            printf("[Sensors] APDS-9960 Hand Gesture Sensor detected at 0x39\n");
        }
    }
}

void sensors_update(SmartSensorsData *data)
{
    if (!data) return;

    // Read Glasses Battery % from MAX17048 or PiSugar 3
    if (data->has_fuel_gauge && fd_fuel >= 0) {
        int soc = is_pisugar
            ? wiringPiI2CReadReg8(fd_fuel, 0x2A)
            : wiringPiI2CReadReg8(fd_fuel, 0x04);
        if (soc >= 0 && soc <= 100) {
            data->glasses_battery_pct = soc;
        }
    }

    // Read Compass Heading from BNO055 Euler Yaw register (0x1A..0x1B, 1/16th deg)
    if (data->has_imu && fd_imu >= 0 && is_bno055) {
        int lsb = wiringPiI2CReadReg8(fd_imu, 0x1A);
        int msb = wiringPiI2CReadReg8(fd_imu, 0x1B);
        if (lsb >= 0 && msb >= 0) {
            int raw = (int16_t)((msb << 8) | lsb);
            int deg = (raw / 16) % 360;
            if (deg < 0) deg += 360;
            data->compass_heading_deg = deg;
        }
    }

    strncpy(data->compass_cardinal,
            sensors_heading_to_cardinal(data->compass_heading_deg),
            sizeof(data->compass_cardinal) - 1);
}

HandGesture sensors_poll_gesture(SmartSensorsData *data)
{
    if (!data || !data->has_gesture || fd_gest < 0) {
        return GESTURE_NONE;
    }

    // Check APDS-9960 GSTATUS (0xAF) bit 0 (GVALID)
    int gstatus = wiringPiI2CReadReg8(fd_gest, 0xAF);
    if (gstatus < 0 || !(gstatus & 0x01)) {
        return GESTURE_NONE;
    }

    int up    = wiringPiI2CReadReg8(fd_gest, 0xFC);
    int down  = wiringPiI2CReadReg8(fd_gest, 0xFD);
    int left  = wiringPiI2CReadReg8(fd_gest, 0xFE);
    int right = wiringPiI2CReadReg8(fd_gest, 0xFF);
    if (up < 0 || down < 0 || left < 0 || right < 0) return GESTURE_NONE;

    int ud_diff = up - down;
    int lr_diff = left - right;

    if (abs(lr_diff) > abs(ud_diff)) {
        if (lr_diff > 30)  return GESTURE_LEFT;
        if (lr_diff < -30) return GESTURE_RIGHT;
    } else {
        if (ud_diff > 30)  return GESTURE_UP;
        if (ud_diff < -30) return GESTURE_DOWN;
    }
    return GESTURE_NONE;
}

typedef struct {
    char filepath[128];
} CamTaskArg;

static void *camera_worker_thread(void *arg)
{
    CamTaskArg *task = (CamTaskArg *)arg;
    if (!task) return NULL;

    char cmd[384];
    // Supports Bookworm (rpicam-still), Bullseye (libcamera-still), and Legacy (raspistill)
    snprintf(cmd, sizeof(cmd),
             "(rpicam-still -n -t 300 --width 1280 --height 720 -o \"%s\" 2>/dev/null) || "
             "(libcamera-still -n -t 300 --width 1280 --height 720 -o \"%s\" 2>/dev/null) || "
             "(raspistill -n -t 300 -w 1280 -h 720 -o \"%s\" 2>/dev/null)",
             task->filepath, task->filepath, task->filepath);
    system(cmd);
    free(task);
    return NULL;
}

int camera_capture_pov_async(SmartSensorsData *data, const char *filename_out)
{
    if (!data) return -1;
    data->photo_count++;
    const char *out = (filename_out && filename_out[0]) ? filename_out : "/tmp/glasses_pov.jpg";
    strncpy(data->last_photo_path, out, sizeof(data->last_photo_path) - 1);
    data->last_photo_path[sizeof(data->last_photo_path) - 1] = '\0';

    CamTaskArg *arg = (CamTaskArg *)malloc(sizeof(CamTaskArg));
    if (!arg) return -1;
    strncpy(arg->filepath, data->last_photo_path, sizeof(arg->filepath) - 1);
    arg->filepath[sizeof(arg->filepath) - 1] = '\0';

    pthread_t tid;
    if (pthread_create(&tid, NULL, camera_worker_thread, arg) == 0) {
        pthread_detach(tid);
        return 0;
    }
    free(arg);
    return -1;
}
