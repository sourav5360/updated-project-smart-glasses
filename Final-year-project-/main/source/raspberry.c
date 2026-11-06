/*
 * Smart Glasses HUD Server for Raspberry Pi Zero W
 * Upgraded from original Final-year-project- (SSD1306 I2C OLED + Bluetooth RFCOMM + WM8960 Audio HAT)
 *
 * Features:
 *  1. Dual Transport: Bluetooth RFCOMM (Channel 1) + Wi-Fi TCP Server (Port 8888)
 *  2. Multi-Screen HUD:
 *     - Live Dashboard (Big Clock, Date, Phone Battery %, Weather, Unread count)
 *     - Rich App Notifications (WhatsApp, SMS, Email, Social) with 16x16 Icons & Ring Buffer
 *     - Turn-by-Turn Navigation (28x28 Maneuver Arrows, Distance, Street Name, ETA)
 *     - Now Playing Media Controller (Track, Artist, Play/Pause, Progress Bar)
 *     - High-Priority Incoming Call Overlay with Caller ID
 *     - 128x64 1-Bit Bitmap / Live Screen Mirroring Projection (BMP|<2048 hex>)
 *  3. WM8960 Audio HAT Voice & TTS Output (espeak / aplay / arecord integration)
 *  4. Smart Glasses Optics Mirror Mode (Horizontal/Vertical flip for combiner glass/prism)
 *  5. 100% Backward Compatible with legacy plain-text & "clear" commands
 *
 * Compile:
 *   sudo gcc raspberry.c ssd1306_i2c.c -lwiringPi -lbluetooth -lpthread -o raspberry
 */

#include "ssd1306_i2c.h"
#include "st7789_spi.h"
#include "hdmi_prism_hud.h"
#include "smart_sensors.h"
#include "hud_icons.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <pthread.h>

#define TCP_PORT 8888
#define MAX_NOTIFS 10
#define MAX_PACKET_LEN 4096

typedef enum {
    HUD_MODE_DASHBOARD = 0,
    HUD_MODE_NOTIFICATION,
    HUD_MODE_NAVIGATION,
    HUD_MODE_MUSIC,
    HUD_MODE_CALL,
    HUD_MODE_CAMERA,
    HUD_MODE_BITMAP,
    HUD_MODE_RAW_TEXT
} HudMode;

typedef struct {
    char app[24];
    char title[48];
    char body[160];
    char timestamp[16];
} NotificationItem;

typedef struct {
    // Connection & system status
    int bt_connected;
    int wifi_connected;
    int active_client_fd;
    int phone_battery;
    char time_str[16];
    char date_str[24];
    char weather_str[24];
    int tts_enabled;
    int tts_speaking;
    int mirror_x;
    int mirror_y;

    // Pro Sensors & Camera state (MAX17048/PiSugar3, BNO055/MPU6050, APDS-9960, Pi Camera)
    SmartSensorsData sensors;
    int hdmi_prism_active;

    // Active HUD state
    HudMode mode;
    HudMode prev_mode;

    // Notification ring buffer
    NotificationItem notifs[MAX_NOTIFS];
    int notif_count;
    int notif_index;

    // Turn-by-turn navigation state
    char nav_dir[24];
    char nav_dist[24];
    char nav_street[48];
    char nav_eta[32];

    // Music / Now Playing state
    int music_playing;
    char music_title[48];
    char music_artist[48];
    int music_progress;

    // Incoming / Active Call state
    char call_state[20];
    char caller_name[48];
    char caller_number[32];

    // Legacy raw text fallback
    char raw_text[256];
} SmartGlassesState;

static SmartGlassesState g_state;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

// Safe string copy helper
static void safe_strcpy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

// Refresh local Raspberry Pi clock if phone hasn't pushed SYS time recently
static void update_local_clock(void)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t) {
        if (g_state.time_str[0] == '\0') {
            strftime(g_state.time_str, sizeof(g_state.time_str), "%H:%M", t);
        }
        if (g_state.date_str[0] == '\0') {
            strftime(g_state.date_str, sizeof(g_state.date_str), "%a %b %d", t);
        }
    }
}

/* =========================================================================
 * WM8960 Audio HAT / TTS Voice Output
 * ========================================================================= */
typedef struct {
    char text[256];
} TtsTaskArg;

static void *tts_worker_thread(void *arg)
{
    TtsTaskArg *task = (TtsTaskArg *)arg;
    if (!task) return NULL;

    pthread_mutex_lock(&g_mutex);
    g_state.tts_speaking = 1;
    pthread_mutex_unlock(&g_mutex);

    // Sanitize text for shell safety (allow alphanumeric, spaces, basic punctuation)
    char clean[256];
    size_t j = 0;
    for (size_t i = 0; task->text[i] != '\0' && j < sizeof(clean) - 1; i++) {
        char c = task->text[i];
        if (isalnum((unsigned char)c) || c == ' ' || c == ',' || c == '.' || c == ':' || c == '-') {
            clean[j++] = c;
        }
    }
    clean[j] = '\0';

    if (clean[0] != '\0') {
        char cmd[384];
        // Uses espeak piped to aplay (configured for WM8960 in /usr/share/alsa/alsa.conf)
        snprintf(cmd, sizeof(cmd),
                 "(espeak -s 155 --stdout \"%s\" 2>/dev/null | aplay -q 2>/dev/null) || espeak -s 155 \"%s\" 2>/dev/null",
                 clean, clean);
        system(cmd);
    }

    pthread_mutex_lock(&g_mutex);
    g_state.tts_speaking = 0;
    pthread_mutex_unlock(&g_mutex);

    free(task);
    return NULL;
}

static void speak_tts_async(const char *text)
{
    if (!text || text[0] == '\0') return;
    TtsTaskArg *arg = (TtsTaskArg *)malloc(sizeof(TtsTaskArg));
    if (!arg) return;
    safe_strcpy(arg->text, sizeof(arg->text), text);

    pthread_t tid;
    if (pthread_create(&tid, NULL, tts_worker_thread, arg) == 0) {
        pthread_detach(tid);
    } else {
        free(arg);
    }
}

/* =========================================================================
 * OLED HUD Rendering Engine (128x64)
 * ========================================================================= */

// Draw persistent 10px top status bar
static void render_status_bar(void)
{
    // Connection icons on left
    int x = 0;
    if (g_state.bt_connected) {
        ssd1306_drawBitmap(x, 0, ICON_BT_8x8, 8, 8, WHITE);
        x += 10;
    }
    if (g_state.wifi_connected) {
        ssd1306_drawBitmap(x, 0, ICON_WIFI_8x8, 8, 8, WHITE);
        x += 10;
    }
    if (g_state.tts_speaking) {
        ssd1306_drawBitmap(x, 0, ICON_SPEAKER_8x8, 8, 8, WHITE);
        x += 10;
    }

    // Time + AR Compass Heading (from BNO055/MPU6050 IMU) in center-left
    char time_compass[28];
    snprintf(time_compass, sizeof(time_compass), "%.5s %s",
             g_state.time_str, g_state.sensors.compass_cardinal);
    ssd1306_drawStringAt(x, 0, time_compass, 1, WHITE);

    // Dual Battery Readout: Glasses Battery (G) & Phone Battery (P) Icon on right
    char batt_buf[12];
    snprintf(batt_buf, sizeof(batt_buf), "%d%%", g_state.phone_battery);
    int batt_text_x = 112 - (int)strlen(batt_buf) * 6;
    ssd1306_drawStringAt(batt_text_x, 0, batt_buf, 1, WHITE);
    hud_drawBattery(114, 0, g_state.phone_battery);

    // Divider line at y = 9
    ssd1306_drawFastHLine(0, 9, 128, WHITE);
}

static void render_dashboard(void)
{
    render_status_bar();

    // Large Clock (size 2 = 12x16 per char)
    ssd1306_drawStringAt(2, 14, g_state.time_str, 2, WHITE);

    // Weather & Glasses Battery % (from PiSugar 3 / MAX17048 Fuel Gauge) on the right
    ssd1306_drawStringAt(68, 14, g_state.weather_str, 1, WHITE);
    char g_batt[20];
    snprintf(g_batt, sizeof(g_batt), "Gls:%d%% %ddeg",
             g_state.sensors.glasses_battery_pct, g_state.sensors.compass_heading_deg);
    ssd1306_drawStringAt(68, 24, g_batt, 1, WHITE);

    ssd1306_drawFastHLine(0, 34, 128, WHITE);

    // Bottom section: show active music or latest notification summary
    if (g_state.music_playing && g_state.music_title[0] != '\0') {
        ssd1306_drawBitmap(2, 38, ICON_MUSIC_8x8, 8, 8, WHITE);
        char line[22];
        snprintf(line, sizeof(line), "%.18s", g_state.music_title);
        ssd1306_drawStringAt(14, 38, line, 1, WHITE);
        ssd1306_drawProgressBar(2, 50, 124, 8, g_state.music_progress);
    } else if (g_state.notif_count > 0) {
        NotificationItem *latest = &g_state.notifs[g_state.notif_index];
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "[%s] %.12s", latest->app, latest->title);
        ssd1306_drawStringAt(2, 38, hdr, 1, WHITE);

        char preview[22];
        snprintf(preview, sizeof(preview), "%.20s", latest->body);
        ssd1306_drawStringAt(2, 49, preview, 1, WHITE);
    } else {
        ssd1306_drawStringAt(8, 40, "SMART GLASSES READY", 1, WHITE);
        ssd1306_drawStringAt(14, 51, "Waiting for phone", 1, WHITE);
    }
}

static void render_notification(void)
{
    render_status_bar();

    if (g_state.notif_count == 0) {
        ssd1306_drawStringAt(16, 30, "No Notifications", 1, WHITE);
        return;
    }

    NotificationItem *item = &g_state.notifs[g_state.notif_index];

    // Choose 16x16 icon based on app type
    const unsigned char *icon = ICON_BELL_16x16;
    if (strcasecmp(item->app, "WHATSAPP") == 0 || strcasecmp(item->app, "SMS") == 0 ||
        strcasecmp(item->app, "CHAT") == 0 || strcasecmp(item->app, "TELEGRAM") == 0) {
        icon = ICON_CHAT_16x16;
    } else if (strcasecmp(item->app, "EMAIL") == 0 || strcasecmp(item->app, "GMAIL") == 0) {
        icon = ICON_MAIL_16x16;
    } else if (strcasecmp(item->app, "CALL") == 0 || strcasecmp(item->app, "PHONE") == 0) {
        icon = ICON_PHONE_16x16;
    }

    ssd1306_drawBitmap(0, 12, icon, 16, 16, WHITE);

    // Inverted header banner for App + Title
    ssd1306_fillRect(18, 12, 110, 10, WHITE);
    char banner[20];
    snprintf(banner, sizeof(banner), "%.17s", item->title[0] ? item->title : item->app);
    ssd1306_drawStringAt(20, 13, banner, 1, BLACK);

    // Notification index badge (e.g. "1/3")
    char badge[16];
    snprintf(badge, sizeof(badge), "%s (%d/%d)", item->app, g_state.notif_index + 1, g_state.notif_count);
    ssd1306_drawStringAt(20, 23, badge, 1, WHITE);

    // Multi-line wrapped message body (up to 3 lines below y=34)
    ssd1306_setCursor(0, 35);
    ssd1306_setTextSize(1);
    ssd1306_setTextColor(WHITE);
    ssd1306_setTextWrap(1);
    char body_trunc[64];
    snprintf(body_trunc, sizeof(body_trunc), "%.60s", item->body);
    ssd1306_drawString(body_trunc);
}

static void render_navigation(void)
{
    render_status_bar();

    // Left: 28x28 Maneuver Arrow at (0, 13)
    hud_drawNavArrow(0, 13, g_state.nav_dir);

    // Right of arrow: Large Distance readout (size 2)
    char dist_short[10];
    snprintf(dist_short, sizeof(dist_short), "%.7s", g_state.nav_dist);
    ssd1306_drawStringAt(32, 13, dist_short, 2, WHITE);

    // Street / Instruction below distance
    char street_line[17];
    snprintf(street_line, sizeof(street_line), "%.15s", g_state.nav_street);
    ssd1306_drawStringAt(32, 32, street_line, 1, WHITE);

    // Bottom inverted bar: ETA / Remaining distance
    ssd1306_fillRect(0, 52, 128, 12, WHITE);
    char eta_line[22];
    snprintf(eta_line, sizeof(eta_line), "ETA: %.15s", g_state.nav_eta);
    ssd1306_drawStringAt(4, 54, eta_line, 1, BLACK);
}

static void render_music(void)
{
    render_status_bar();

    ssd1306_drawBitmap(2, 14, ICON_MUSIC_8x8, 8, 8, WHITE);
    ssd1306_drawStringAt(14, 14, g_state.music_playing ? "NOW PLAYING" : "PAUSED", 1, WHITE);

    // Track title & artist
    char title_line[22];
    snprintf(title_line, sizeof(title_line), "%.20s", g_state.music_title);
    ssd1306_drawStringAt(2, 26, title_line, 1, WHITE);

    char artist_line[22];
    snprintf(artist_line, sizeof(artist_line), "%.20s", g_state.music_artist);
    ssd1306_drawStringAt(2, 37, artist_line, 1, WHITE);

    // Progress bar at bottom
    ssd1306_drawProgressBar(2, 50, 124, 10, g_state.music_progress);
}

static void render_call(void)
{
    // High-visibility Incoming/Active Call Overlay
    ssd1306_fillRect(0, 0, 128, 14, WHITE);
    char hdr[22];
    snprintf(hdr, sizeof(hdr), "CALL: %.14s", g_state.call_state);
    ssd1306_drawStringAt(6, 3, hdr, 1, BLACK);

    ssd1306_drawBitmap(4, 20, ICON_PHONE_16x16, 16, 16, WHITE);

    char caller[18];
    snprintf(caller, sizeof(caller), "%.16s", g_state.caller_name);
    ssd1306_drawStringAt(24, 20, caller, 1, WHITE);

    char num[18];
    snprintf(num, sizeof(num), "%.16s", g_state.caller_number);
    ssd1306_drawStringAt(24, 32, num, 1, WHITE);

    ssd1306_drawRect(0, 48, 128, 16, WHITE);
    ssd1306_drawStringAt(10, 52, " Audio via WM8960 ", 1, WHITE);
}

static void render_camera(void)
{
    render_status_bar();
    // Viewfinder crosshair & POV camera status
    ssd1306_drawRect(4, 13, 120, 48, WHITE);
    ssd1306_drawFastHLine(58, 36, 12, WHITE);
    ssd1306_drawFastVLine(64, 30, 12, WHITE);
    ssd1306_drawStringAt(12, 17, "POV CAMERA SNAP", 1, WHITE);
    char info[24];
    snprintf(info, sizeof(info), "Saved #%d (720p)", g_state.sensors.photo_count);
    ssd1306_drawStringAt(12, 47, info, 1, WHITE);
}

static void render_raw_text(void)
{
    render_status_bar();
    ssd1306_setCursor(0, 14);
    ssd1306_setTextSize(1);
    ssd1306_setTextColor(WHITE);
    ssd1306_setTextWrap(1);
    ssd1306_drawString(g_state.raw_text);
}

// Render 640x400 Full-Color AR HUD to 0.23" HDMI Micro-OLED Optical Prism (/dev/fb0)
static void hud_refresh_hdmi_prism_locked(void)
{
    if (!g_state.hdmi_prism_active) return;

    // Pure black = 100% see-through inside the 0.23" Optical Prism
    hdmi_prism_clear(ARGB_TRANSPARENT_BLACK);

    // Top Status & AR Compass Bar (640x42)
    hdmi_prism_fillRect(0, 0, 640, 42, ARGB_DARK_CARD_BG);
    hdmi_prism_drawRect(0, 0, 640, 42, 2, ARGB_HUD_CYAN);

    char top_left[64];
    snprintf(top_left, sizeof(top_left), "%s | %s | %s",
             g_state.time_str, g_state.date_str, g_state.weather_str);
    hdmi_prism_drawStringAt(12, 12, top_left, 2, ARGB_HUD_WHITE);

    char compass_tape[48];
    snprintf(compass_tape, sizeof(compass_tape), "< %03d deg %s >",
             g_state.sensors.compass_heading_deg, g_state.sensors.compass_cardinal);
    hdmi_prism_drawStringAt(270, 12, compass_tape, 2, ARGB_AMBER_GOLD);

    char dual_batt[48];
    snprintf(dual_batt, sizeof(dual_batt), "P:%d%% G:%d%%",
             g_state.phone_battery, g_state.sensors.glasses_battery_pct);
    hdmi_prism_drawStringAt(490, 12, dual_batt, 2, ARGB_WHATSAPP_GREEN);

    // Main AR Viewport (y = 60..380)
    if (g_state.mode == HUD_MODE_DASHBOARD) {
        hdmi_prism_drawStringAt(40, 90, g_state.time_str, 6, ARGB_HUD_CYAN);
        hdmi_prism_drawStringAt(40, 175, "0.23\" HDMI MICRO-OLED PRISM READY", 2, ARGB_HUD_WHITE);
        hdmi_prism_drawStringAt(40, 210, "Bone Conduction Audio + Gesture + IMU Active", 2, ARGB_AMBER_GOLD);
    } else if (g_state.mode == HUD_MODE_NOTIFICATION && g_state.notif_count > 0) {
        NotificationItem *item = &g_state.notifs[g_state.notif_index];
        hdmi_prism_drawRect(30, 70, 580, 260, 3, ARGB_WHATSAPP_GREEN);
        hdmi_prism_fillRect(30, 70, 580, 44, ARGB_WHATSAPP_GREEN);
        hdmi_prism_drawStringAt(44, 82, item->title, 3, ARGB_TRANSPARENT_BLACK);
        hdmi_prism_drawStringAt(44, 140, item->body, 3, ARGB_HUD_WHITE);
    } else if (g_state.mode == HUD_MODE_NAVIGATION) {
        hdmi_prism_drawRect(30, 70, 580, 260, 3, ARGB_MAPS_BLUE);
        hdmi_prism_drawStringAt(50, 95, g_state.nav_dir, 4, ARGB_AMBER_GOLD);
        hdmi_prism_drawStringAt(50, 155, g_state.nav_dist, 5, ARGB_HUD_WHITE);
        hdmi_prism_drawStringAt(50, 225, g_state.nav_street, 3, ARGB_HUD_CYAN);
        hdmi_prism_fillRect(30, 285, 580, 45, ARGB_MAPS_BLUE);
        hdmi_prism_drawStringAt(50, 298, g_state.nav_eta, 3, ARGB_HUD_WHITE);
    } else if (g_state.mode == HUD_MODE_MUSIC) {
        hdmi_prism_drawRect(30, 70, 580, 260, 3, ARGB_SPOTIFY_GREEN);
        hdmi_prism_drawStringAt(50, 95, g_state.music_playing ? "NOW PLAYING" : "PAUSED", 3, ARGB_SPOTIFY_GREEN);
        hdmi_prism_drawStringAt(50, 150, g_state.music_title, 4, ARGB_HUD_WHITE);
        hdmi_prism_drawStringAt(50, 205, g_state.music_artist, 3, ARGB_HUD_CYAN);
        hdmi_prism_drawProgressBar(50, 265, 540, 32, g_state.music_progress, ARGB_SPOTIFY_GREEN, ARGB_HUD_WHITE);
    } else if (g_state.mode == HUD_MODE_CALL) {
        hdmi_prism_drawRect(30, 70, 580, 260, 4, ARGB_ALERT_RED);
        hdmi_prism_fillRect(30, 70, 580, 50, ARGB_ALERT_RED);
        hdmi_prism_drawStringAt(50, 84, "INCOMING CALL (BONE CONDUCTION)", 3, ARGB_HUD_WHITE);
        hdmi_prism_drawStringAt(50, 155, g_state.caller_name, 4, ARGB_HUD_WHITE);
        hdmi_prism_drawStringAt(50, 215, g_state.caller_number, 3, ARGB_AMBER_GOLD);
    } else if (g_state.mode == HUD_MODE_CAMERA) {
        hdmi_prism_drawRect(60, 70, 520, 280, 3, ARGB_AMBER_GOLD);
        hdmi_prism_drawStringAt(160, 195, "+ POV CAMERA CAPTURED +", 3, ARGB_HUD_CYAN);
        hdmi_prism_drawStringAt(160, 245, g_state.sensors.last_photo_path, 2, ARGB_HUD_WHITE);
    }

    hdmi_prism_display();
}

// Render the current state to both SSD1306 OLED & 0.23" HDMI Micro-OLED Prism (must be called with g_mutex locked)
static void hud_refresh_locked(void)
{
    update_local_clock();
    hud_refresh_hdmi_prism_locked();

    if (g_state.mode == HUD_MODE_BITMAP) {
        // Bitmap buffer is already populated by ssd1306_drawFullFrameHex
        ssd1306_display();
        return;
    }

    ssd1306_clearDisplay();

    switch (g_state.mode) {
    case HUD_MODE_DASHBOARD:
        render_dashboard();
        break;
    case HUD_MODE_NOTIFICATION:
        render_notification();
        break;
    case HUD_MODE_NAVIGATION:
        render_navigation();
        break;
    case HUD_MODE_MUSIC:
        render_music();
        break;
    case HUD_MODE_CALL:
        render_call();
        break;
    case HUD_MODE_CAMERA:
        render_camera();
        break;
    case HUD_MODE_RAW_TEXT:
    default:
        render_raw_text();
        break;
    }

    ssd1306_display();
}

/* =========================================================================
 * Structured Protocol Parser
 * ========================================================================= */

// Split a mutable string by '|' into tokens
static int split_pipe_tokens(char *str, char **tokens, int max_tokens)
{
    int count = 0;
    char *p = str;
    while (p && count < max_tokens) {
        tokens[count++] = p;
        char *sep = strchr(p, '|');
        if (!sep) break;
        *sep = '\0';
        p = sep + 1;
    }
    return count;
}

static void push_notification_locked(const char *app, const char *title, const char *body)
{
    // Shift ring buffer if full
    if (g_state.notif_count == MAX_NOTIFS) {
        memmove(&g_state.notifs[0], &g_state.notifs[1], sizeof(NotificationItem) * (MAX_NOTIFS - 1));
        g_state.notif_count--;
    }
    int idx = g_state.notif_count++;
    safe_strcpy(g_state.notifs[idx].app, sizeof(g_state.notifs[idx].app), app);
    safe_strcpy(g_state.notifs[idx].title, sizeof(g_state.notifs[idx].title), title);
    safe_strcpy(g_state.notifs[idx].body, sizeof(g_state.notifs[idx].body), body);
    safe_strcpy(g_state.notifs[idx].timestamp, sizeof(g_state.notifs[idx].timestamp), g_state.time_str);
    g_state.notif_index = idx;
}

static void handle_command_line(char *line)
{
    // Trim trailing CR/LF
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
        line[--len] = '\0';
    }
    if (len == 0) return;

    printf("[Phone -> Glasses] %.120s%s\n", line, len > 120 ? "..." : "");

    // Check legacy "clear" command first
    if (strcasecmp(line, "clear") == 0) {
        pthread_mutex_lock(&g_mutex);
        ssd1306_clearDisplay();
        ssd1306_display(); // Fixed bug in original raspberry.c where ssd1306_display() was omitted
        pthread_mutex_unlock(&g_mutex);
        return;
    }

    // If no '|' delimiter is present, handle single-word commands or legacy raw text
    if (strchr(line, '|') == NULL) {
        pthread_mutex_lock(&g_mutex);
        if (strcasecmp(line, "NEXT") == 0 && g_state.notif_count > 0) {
            g_state.notif_index = (g_state.notif_index + 1) % g_state.notif_count;
            g_state.mode = HUD_MODE_NOTIFICATION;
        } else if (strcasecmp(line, "PREV") == 0 && g_state.notif_count > 0) {
            g_state.notif_index = (g_state.notif_index - 1 + g_state.notif_count) % g_state.notif_count;
            g_state.mode = HUD_MODE_NOTIFICATION;
        } else if (strcasecmp(line, "DASHBOARD") == 0 || strcasecmp(line, "HOME") == 0) {
            g_state.mode = HUD_MODE_DASHBOARD;
        } else {
            // Backward-compatible raw text mode
            safe_strcpy(g_state.raw_text, sizeof(g_state.raw_text), line);
            g_state.mode = HUD_MODE_RAW_TEXT;
        }
        hud_refresh_locked();
        pthread_mutex_unlock(&g_mutex);
        return;
    }

    char *tokens[8] = {0};
    int n = split_pipe_tokens(line, tokens, 8);
    if (n <= 0) return;

    const char *cmd = tokens[0];
    char tts_buf[256] = {0};

    pthread_mutex_lock(&g_mutex);

    if (strcasecmp(cmd, "SYS") == 0) {
        // SYS|<HH:MM>|<Date>|<PhoneBattery%>|<Weather>
        if (n > 1 && tokens[1][0]) safe_strcpy(g_state.time_str, sizeof(g_state.time_str), tokens[1]);
        if (n > 2 && tokens[2][0]) safe_strcpy(g_state.date_str, sizeof(g_state.date_str), tokens[2]);
        if (n > 3 && tokens[3][0]) g_state.phone_battery = atoi(tokens[3]);
        if (n > 4 && tokens[4][0]) safe_strcpy(g_state.weather_str, sizeof(g_state.weather_str), tokens[4]);
        hud_refresh_locked();
    } else if (strcasecmp(cmd, "NOTIF") == 0) {
        // NOTIF|<app>|<title>|<body>
        const char *app = (n > 1) ? tokens[1] : "APP";
        const char *title = (n > 2) ? tokens[2] : "Notification";
        const char *body = (n > 3) ? tokens[3] : "";
        push_notification_locked(app, title, body);
        g_state.mode = HUD_MODE_NOTIFICATION;
        hud_refresh_locked();
        if (g_state.tts_enabled) {
            snprintf(tts_buf, sizeof(tts_buf), "%s from %s. %s", app, title, body);
        }
    } else if (strcasecmp(cmd, "NAV") == 0) {
        // NAV|<dir>|<dist>|<street>|<eta>
        safe_strcpy(g_state.nav_dir, sizeof(g_state.nav_dir), (n > 1) ? tokens[1] : "STRAIGHT");
        safe_strcpy(g_state.nav_dist, sizeof(g_state.nav_dist), (n > 2) ? tokens[2] : "0 m");
        safe_strcpy(g_state.nav_street, sizeof(g_state.nav_street), (n > 3) ? tokens[3] : "Continue");
        safe_strcpy(g_state.nav_eta, sizeof(g_state.nav_eta), (n > 4) ? tokens[4] : "--:--");
        g_state.mode = HUD_MODE_NAVIGATION;
        hud_refresh_locked();
    } else if (strcasecmp(cmd, "MUSIC") == 0) {
        // MUSIC|<PLAY/PAUSE>|<title>|<artist>|<progress_pct>
        g_state.music_playing = (n > 1 && strcasecmp(tokens[1], "PLAY") == 0) ? 1 : 0;
        safe_strcpy(g_state.music_title, sizeof(g_state.music_title), (n > 2) ? tokens[2] : "Unknown Track");
        safe_strcpy(g_state.music_artist, sizeof(g_state.music_artist), (n > 3) ? tokens[3] : "Unknown Artist");
        g_state.music_progress = (n > 4) ? atoi(tokens[4]) : 0;
        g_state.mode = HUD_MODE_MUSIC;
        hud_refresh_locked();
    } else if (strcasecmp(cmd, "CALL") == 0) {
        // CALL|<RINGING/ACTIVE/ENDED>|<caller_name>|<number>
        const char *cstate = (n > 1) ? tokens[1] : "RINGING";
        if (strcasecmp(cstate, "ENDED") == 0 || strcasecmp(cstate, "IDLE") == 0) {
            g_state.mode = HUD_MODE_DASHBOARD;
        } else {
            safe_strcpy(g_state.call_state, sizeof(g_state.call_state), cstate);
            safe_strcpy(g_state.caller_name, sizeof(g_state.caller_name), (n > 2) ? tokens[2] : "Unknown Caller");
            safe_strcpy(g_state.caller_number, sizeof(g_state.caller_number), (n > 3) ? tokens[3] : "");
            g_state.mode = HUD_MODE_CALL;
            if (g_state.tts_enabled && strcasecmp(cstate, "RINGING") == 0) {
                snprintf(tts_buf, sizeof(tts_buf), "Incoming call from %s", g_state.caller_name);
            }
        }
        hud_refresh_locked();
    } else if (strcasecmp(cmd, "BMP") == 0) {
        // BMP|<2048 hex chars representing 128x64 1-bit frame>
        if (n > 1 && strlen(tokens[1]) >= 2048) {
            if (ssd1306_drawFullFrameHex(tokens[1]) == 0) {
                g_state.mode = HUD_MODE_BITMAP;
                hud_refresh_locked();
            }
        }
    } else if (strcasecmp(cmd, "TTS") == 0) {
        // TTS|<text_to_speak_or_ON/OFF>
        if (n > 1) {
            if (strcasecmp(tokens[1], "ON") == 0) {
                g_state.tts_enabled = 1;
            } else if (strcasecmp(tokens[1], "OFF") == 0) {
                g_state.tts_enabled = 0;
            } else {
                safe_strcpy(tts_buf, sizeof(tts_buf), tokens[1]);
            }
        }
        hud_refresh_locked();
    } else if (strcasecmp(cmd, "MIRROR") == 0) {
        // MIRROR|<mirror_x:0/1>|<mirror_y:0/1> (for Smart Glasses combiner optics)
        g_state.mirror_x = (n > 1) ? atoi(tokens[1]) : 0;
        g_state.mirror_y = (n > 2) ? atoi(tokens[2]) : 0;
        ssd1306_setMirror(g_state.mirror_x, g_state.mirror_y);
        hdmi_prism_setMirror(g_state.mirror_x, g_state.mirror_y);
        hud_refresh_locked();
    } else if (strcasecmp(cmd, "CAM") == 0) {
        // CAM|SNAP|<optional_path> -> Trigger Pi Camera Module 3 / ZeroCam POV snapshot
        const char *path = (n > 2 && tokens[2][0]) ? tokens[2] : "/tmp/glasses_pov.jpg";
        camera_capture_pov_async(&g_state.sensors, path);
        g_state.mode = HUD_MODE_CAMERA;
        hud_refresh_locked();
        if (g_state.tts_enabled) {
            snprintf(tts_buf, sizeof(tts_buf), "POV photo captured");
        }
        if (g_state.active_client_fd >= 0) {
            char evt[192];
            snprintf(evt, sizeof(evt), "EVENT|PHOTO_CAPTURED|%s\n", g_state.sensors.last_photo_path);
            write(g_state.active_client_fd, evt, strlen(evt));
        }
    } else if (strcasecmp(cmd, "COMPASS") == 0 && n > 1) {
        // COMPASS|<0-359> -> Update AR Compass heading (from BNO055/MPU6050 or Phone Magnetometer)
        int deg = ((atoi(tokens[1]) % 360) + 360) % 360;
        g_state.sensors.compass_heading_deg = deg;
        safe_strcpy(g_state.sensors.compass_cardinal,
                    sizeof(g_state.sensors.compass_cardinal),
                    sensors_heading_to_cardinal(deg));
        hud_refresh_locked();
    } else if (strcasecmp(cmd, "GLASSES_BATT") == 0 && n > 1) {
        // GLASSES_BATT|<0-100> -> Update Glasses Battery % (from PiSugar 3 / MAX17048)
        g_state.sensors.glasses_battery_pct = atoi(tokens[1]);
        hud_refresh_locked();
    } else if (strcasecmp(cmd, "GESTURE") == 0 && n > 1) {
        // GESTURE|<LEFT/RIGHT/UP/DOWN> -> Hand wave from APDS-9960 or phone test button
        if (strcasecmp(tokens[1], "RIGHT") == 0) {
            g_state.mode = (HudMode)((g_state.mode + 1) % 4);
        } else if (strcasecmp(tokens[1], "LEFT") == 0) {
            g_state.mode = (HudMode)((g_state.mode + 3) % 4);
        } else if (strcasecmp(tokens[1], "UP") == 0) {
            camera_capture_pov_async(&g_state.sensors, "/tmp/glasses_pov.jpg");
            g_state.mode = HUD_MODE_CAMERA;
        } else if (strcasecmp(tokens[1], "DOWN") == 0) {
            g_state.mode = HUD_MODE_DASHBOARD;
        }
        hud_refresh_locked();
    } else if (strcasecmp(cmd, "BRIGHTNESS") == 0 || strcasecmp(cmd, "CONTRAST") == 0) {
        // BRIGHTNESS|<0-255>
        if (n > 1) {
            ssd1306_setContrast((unsigned int)atoi(tokens[1]));
        }
    } else if (strcasecmp(cmd, "DIM") == 0) {
        if (n > 1) {
            ssd1306_dim(atoi(tokens[1]) ? 1 : 0);
        }
    } else if (strcasecmp(cmd, "MODE") == 0 && n > 1) {
        if (strcasecmp(tokens[1], "DASHBOARD") == 0) g_state.mode = HUD_MODE_DASHBOARD;
        else if (strcasecmp(tokens[1], "NOTIF") == 0) g_state.mode = HUD_MODE_NOTIFICATION;
        else if (strcasecmp(tokens[1], "NAV") == 0) g_state.mode = HUD_MODE_NAVIGATION;
        else if (strcasecmp(tokens[1], "MUSIC") == 0) g_state.mode = HUD_MODE_MUSIC;
        else if (strcasecmp(tokens[1], "CAMERA") == 0) g_state.mode = HUD_MODE_CAMERA;
        hud_refresh_locked();
    } else {
        // Fallback: display raw text
        safe_strcpy(g_state.raw_text, sizeof(g_state.raw_text), line);
        g_state.mode = HUD_MODE_RAW_TEXT;
        hud_refresh_locked();
    }

    pthread_mutex_unlock(&g_mutex);

    if (tts_buf[0] != '\0') {
        speak_tts_async(tts_buf);
    }
}

/* =========================================================================
 * Client Stream Reader & Bidirectional Sender Threads
 * ========================================================================= */
typedef struct {
    int client_fd;
    int is_bluetooth;
} ClientSession;

static void *client_recv_thread(void *arg)
{
    ClientSession *sess = (ClientSession *)arg;
    int fd = sess->client_fd;
    int is_bt = sess->is_bluetooth;
    free(sess);

    char accum[MAX_PACKET_LEN];
    size_t accum_len = 0;
    char chunk[1024];

    while (1) {
        ssize_t rc = read(fd, chunk, sizeof(chunk) - 1);
        if (rc <= 0) {
            break;
        }
        chunk[rc] = '\0';

        // Append to line accumulator so large packets (like BMP|<2048 hex>) reassemble cleanly
        for (ssize_t i = 0; i < rc; i++) {
            char c = chunk[i];
            if (c == '\n') {
                accum[accum_len] = '\0';
                handle_command_line(accum);
                accum_len = 0;
            } else if (accum_len < sizeof(accum) - 1) {
                accum[accum_len++] = c;
            }
        }

        // Support legacy Android app that sends short text without trailing newline
        if (accum_len > 0 && accum_len < 256 && strchr(accum, '|') == NULL) {
            accum[accum_len] = '\0';
            handle_command_line(accum);
            accum_len = 0;
        }
    }

    close(fd);

    pthread_mutex_lock(&g_mutex);
    if (is_bt) g_state.bt_connected = 0;
    else g_state.wifi_connected = 0;
    if (g_state.active_client_fd == fd) {
        g_state.active_client_fd = -1;
    }
    hud_refresh_locked();
    pthread_mutex_unlock(&g_mutex);

    printf("[%s] Client disconnected.\n", is_bt ? "Bluetooth" : "Wi-Fi TCP");
    return NULL;
}

// Forward STDIN console commands from Raspberry Pi to connected phone
static void *stdin_sender_thread(void *arg)
{
    (void)arg;
    char s[1024];
    while (1) {
        memset(s, 0, sizeof(s));
        ssize_t n = read(STDIN_FILENO, s, sizeof(s) - 1);
        if (n <= 0) {
            sleep(1);
            continue;
        }
        pthread_mutex_lock(&g_mutex);
        int fd = g_state.active_client_fd;
        pthread_mutex_unlock(&g_mutex);

        if (fd >= 0) {
            write(fd, s, strlen(s));
        }
        // Also allow local testing by typing protocol commands directly in Pi terminal!
        if (strchr(s, '|') != NULL || strncasecmp(s, "MODE", 4) == 0 ||
            strncasecmp(s, "NEXT", 4) == 0 || strncasecmp(s, "DASHBOARD", 9) == 0) {
            handle_command_line(s);
        }
    }
    return NULL;
}

// Wi-Fi TCP Server Thread (Port 8888) for high-speed projection & easy testing
static void *wifi_tcp_server_thread(void *arg)
{
    (void)arg;
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("TCP socket failed");
        return NULL;
    }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(TCP_PORT);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("TCP bind failed");
        close(srv);
        return NULL;
    }

    listen(srv, 4);
    printf("[Wi-Fi HUD Server] Listening on TCP port %d\n", TCP_PORT);

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int client = accept(srv, (struct sockaddr *)&cli_addr, &cli_len);
        if (client < 0) continue;

        printf("[Wi-Fi HUD Server] Accepted connection from %s\n", inet_ntoa(cli_addr.sin_addr));

        pthread_mutex_lock(&g_mutex);
        g_state.wifi_connected = 1;
        g_state.active_client_fd = client;
        hud_refresh_locked();
        pthread_mutex_unlock(&g_mutex);

        ClientSession *sess = (ClientSession *)malloc(sizeof(ClientSession));
        if (sess) {
            sess->client_fd = client;
            sess->is_bluetooth = 0;
            pthread_t tid;
            pthread_create(&tid, NULL, client_recv_thread, sess);
            pthread_detach(tid);
        } else {
            close(client);
        }
    }
    return NULL;
}

// Background hardware sensor polling thread (APDS-9960 Gesture, BNO055/MPU6050 IMU, MAX17048/PiSugar Battery)
static void *sensors_polling_thread(void *arg)
{
    (void)arg;
    int tick = 0;
    while (1) {
        usleep(120000); // Poll APDS-9960 gestures at ~8 Hz
        pthread_mutex_lock(&g_mutex);
        HandGesture g = sensors_poll_gesture(&g_state.sensors);
        if (g != GESTURE_NONE) {
            if (g == GESTURE_RIGHT) g_state.mode = (HudMode)((g_state.mode + 1) % 4);
            else if (g == GESTURE_LEFT) g_state.mode = (HudMode)((g_state.mode + 3) % 4);
            else if (g == GESTURE_UP) {
                camera_capture_pov_async(&g_state.sensors, "/tmp/glasses_pov.jpg");
                g_state.mode = HUD_MODE_CAMERA;
            } else if (g == GESTURE_DOWN) {
                g_state.mode = HUD_MODE_DASHBOARD;
            }
            hud_refresh_locked();
        }
        // Update IMU Compass & Glasses Battery Fuel Gauge every ~1.2 seconds
        if (++tick >= 10) {
            tick = 0;
            if (g_state.sensors.has_fuel_gauge || g_state.sensors.has_imu) {
                sensors_update(&g_state.sensors);
                hud_refresh_locked();
            }
        }
        pthread_mutex_unlock(&g_mutex);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // Initialize default HUD state
    memset(&g_state, 0, sizeof(g_state));
    g_state.active_client_fd = -1;
    g_state.phone_battery = 100;
    g_state.tts_enabled = 1;
    safe_strcpy(g_state.weather_str, sizeof(g_state.weather_str), "24C Clear");
    g_state.mode = HUD_MODE_DASHBOARD;

    // Initialize Pro I2C Sensors (MAX17048/PiSugar3, BNO055/MPU6050 IMU, APDS-9960 Gesture)
    sensors_init(&g_state.sensors);

    // Initialize 0.23" HDMI Micro-OLED Optical Prism (/dev/fb0) & SSD1306 I2C OLED
    if (hdmi_prism_begin("/dev/fb0") == 0) {
        g_state.hdmi_prism_active = 1;
        printf("[HDMI Prism] 0.23\" Micro-OLED Optical Prism active on /dev/fb0 (640x400 ARGB)\n");
    }
    ssd1306_begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDRESS);
    pthread_mutex_lock(&g_mutex);
    hud_refresh_locked();
    pthread_mutex_unlock(&g_mutex);

    // Start background threads for STDIN, Wi-Fi TCP Server, and Pro Hardware Sensors
    pthread_t stdin_tid, tcp_tid, sensor_tid;
    pthread_create(&stdin_tid, NULL, stdin_sender_thread, NULL);
    pthread_detach(stdin_tid);

    pthread_create(&tcp_tid, NULL, wifi_tcp_server_thread, NULL);
    pthread_detach(tcp_tid);

    pthread_create(&sensor_tid, NULL, sensors_polling_thread, NULL);
    pthread_detach(sensor_tid);

    // Allocate Bluetooth RFCOMM server socket (Channel 1)
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char addr_buf[64] = { 0 };
    socklen_t opt = sizeof(rem_addr);

    int bt_sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (bt_sock < 0) {
        perror("[Bluetooth] RFCOMM socket unavailable (continuing in Wi-Fi/Console mode)");
        while (1) sleep(10);
        return 0;
    }

    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = *BDADDR_ANY;
    loc_addr.rc_channel = (uint8_t) 1;

    if (bind(bt_sock, (struct sockaddr *)&loc_addr, sizeof(loc_addr)) < 0) {
        perror("[Bluetooth] RFCOMM bind failed");
    }
    listen(bt_sock, 2);
    printf("[Bluetooth HUD Server] Listening on RFCOMM Channel 1...\n");

    while (1) {
        int client = accept(bt_sock, (struct sockaddr *)&rem_addr, &opt);
        if (client < 0) {
            sleep(1);
            continue;
        }

        ba2str(&rem_addr.rc_bdaddr, addr_buf);
        fprintf(stderr, "[Bluetooth] Accepted connection from %s\n", addr_buf);

        pthread_mutex_lock(&g_mutex);
        g_state.bt_connected = 1;
        g_state.active_client_fd = client;
        hud_refresh_locked();
        pthread_mutex_unlock(&g_mutex);

        ClientSession *sess = (ClientSession *)malloc(sizeof(ClientSession));
        if (sess) {
            sess->client_fd = client;
            sess->is_bluetooth = 1;
            pthread_t tid;
            pthread_create(&tid, NULL, client_recv_thread, sess);
            pthread_detach(tid);
        } else {
            close(client);
        }
    }

    close(bt_sock);
    return 0;
}
