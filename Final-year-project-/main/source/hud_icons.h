#ifndef HUD_ICONS_H_
#define HUD_ICONS_H_

#include "ssd1306_i2c.h"
#include <string.h>

// 8x8 Bluetooth rune icon
static const unsigned char ICON_BT_8x8[] = {
    0x18, // ...##...
    0x14, // ...#.#..
    0x52, // .#.#..#.
    0x3C, // ..####..
    0x3C, // ..####..
    0x52, // .#.#..#.
    0x14, // ...#.#..
    0x18  // ...##...
};

// 8x8 Wi-Fi icon
static const unsigned char ICON_WIFI_8x8[] = {
    0x3C, // ..####..
    0x42, // .#....#.
    0x99, // #..##..#
    0x24, // ..#..#..
    0x00, // ........
    0x18, // ...##...
    0x18, // ...##...
    0x00  // ........
};

// 8x8 Speaker / TTS active icon
static const unsigned char ICON_SPEAKER_8x8[] = {
    0x08, // ....#...
    0x1A, // ...##.#.
    0x79, // .####..#
    0x7D, // .#####.#
    0x79, // .####..#
    0x1A, // ...##.#.
    0x08, // ....#...
    0x00  // ........
};

// 8x8 Musical note icon
static const unsigned char ICON_MUSIC_8x8[] = {
    0x0F, // ....####
    0x09, // ....#..#
    0x09, // ....#..#
    0x09, // ....#..#
    0x39, // ..###..#
    0x7F, // .#######
    0x77, // .###.###
    0x06  // .....##.
};

// 16x16 Chat / WhatsApp / SMS Bubble Icon
static const unsigned char ICON_CHAT_16x16[] = {
    0x07, 0xE0,
    0x1F, 0xF8,
    0x38, 0x1C,
    0x60, 0x06,
    0x4F, 0xF2,
    0x40, 0x02,
    0x4F, 0xF2,
    0x40, 0x02,
    0x4F, 0x82,
    0x60, 0x06,
    0x38, 0x1C,
    0x1F, 0xF8,
    0x1C, 0x00,
    0x38, 0x00,
    0x20, 0x00,
    0x00, 0x00
};

// 16x16 Mail / Email Envelope Icon
static const unsigned char ICON_MAIL_16x16[] = {
    0x00, 0x00,
    0x00, 0x00,
    0x7F, 0xFE,
    0x60, 0x06,
    0x50, 0x0A,
    0x48, 0x12,
    0x44, 0x22,
    0x42, 0x42,
    0x41, 0x82,
    0x40, 0x02,
    0x40, 0x02,
    0x40, 0x02,
    0x7F, 0xFE,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00
};

// 16x16 Phone Call Handset Icon
static const unsigned char ICON_PHONE_16x16[] = {
    0x00, 0x00,
    0x3C, 0x00,
    0x7E, 0x00,
    0x7E, 0x00,
    0x3C, 0x00,
    0x38, 0x00,
    0x1C, 0x00,
    0x0E, 0x00,
    0x07, 0x00,
    0x03, 0x80,
    0x01, 0xDC,
    0x00, 0xFE,
    0x00, 0x7E,
    0x00, 0x3C,
    0x00, 0x00,
    0x00, 0x00
};

// 16x16 Alert / Bell Notification Icon
static const unsigned char ICON_BELL_16x16[] = {
    0x01, 0x80,
    0x03, 0xC0,
    0x0F, 0xF0,
    0x1C, 0x38,
    0x18, 0x18,
    0x18, 0x18,
    0x18, 0x18,
    0x18, 0x18,
    0x30, 0x0C,
    0x30, 0x0C,
    0x7F, 0xFE,
    0x7F, 0xFE,
    0x00, 0x00,
    0x03, 0xC0,
    0x01, 0x80,
    0x00, 0x00
};

// Helper to draw a thick line (3px wide) for crisp Turn-by-Turn Navigation arrows
static inline void hud_drawThickLine(int x0, int y0, int x1, int y1, unsigned int color)
{
    ssd1306_drawLine(x0, y0, x1, y1, color);
    ssd1306_drawLine(x0 + 1, y0, x1 + 1, y1, color);
    ssd1306_drawLine(x0, y0 + 1, x1, y1 + 1, color);
}

// Draw a 28x28 Turn-by-Turn Navigation Maneuver Arrow at (x, y)
static inline void hud_drawNavArrow(int x, int y, const char *maneuver)
{
    ssd1306_drawRect(x, y, 28, 28, WHITE);
    int cx = x + 14;
    int cy = y + 14;

    if (!maneuver || strcasecmp(maneuver, "STRAIGHT") == 0 || strcasecmp(maneuver, "UP") == 0) {
        // Up arrow
        ssd1306_fillRect(cx - 2, y + 6, 4, 18, WHITE);
        hud_drawThickLine(cx, y + 4, cx - 7, y + 11, WHITE);
        hud_drawThickLine(cx, y + 4, cx + 7, y + 11, WHITE);
    } else if (strcasecmp(maneuver, "LEFT") == 0) {
        // Up then sharp 90-deg Left turn
        ssd1306_fillRect(cx + 2, cy - 2, 4, 12, WHITE);
        ssd1306_fillRect(x + 6, cy - 2, 14, 4, WHITE);
        hud_drawThickLine(x + 4, cy, x + 11, cy - 6, WHITE);
        hud_drawThickLine(x + 4, cy, x + 11, cy + 6, WHITE);
    } else if (strcasecmp(maneuver, "RIGHT") == 0) {
        // Up then sharp 90-deg Right turn
        ssd1306_fillRect(cx - 6, cy - 2, 4, 12, WHITE);
        ssd1306_fillRect(cx - 6, cy - 2, 14, 4, WHITE);
        hud_drawThickLine(x + 23, cy, x + 16, cy - 6, WHITE);
        hud_drawThickLine(x + 23, cy, x + 16, cy + 6, WHITE);
    } else if (strcasecmp(maneuver, "SLIGHT_LEFT") == 0) {
        // Diagonal up-left arrow
        hud_drawThickLine(cx + 5, y + 22, x + 6, y + 6, WHITE);
        hud_drawThickLine(x + 6, y + 6, x + 15, y + 6, WHITE);
        hud_drawThickLine(x + 6, y + 6, x + 6, y + 15, WHITE);
    } else if (strcasecmp(maneuver, "SLIGHT_RIGHT") == 0) {
        // Diagonal up-right arrow
        hud_drawThickLine(cx - 5, y + 22, x + 21, y + 6, WHITE);
        hud_drawThickLine(x + 21, y + 6, x + 12, y + 6, WHITE);
        hud_drawThickLine(x + 21, y + 6, x + 21, y + 15, WHITE);
    } else if (strcasecmp(maneuver, "UTURN") == 0) {
        // U-turn arrow
        ssd1306_fillRect(cx + 3, y + 8, 3, 15, WHITE);
        ssd1306_fillRect(cx - 5, y + 6, 11, 3, WHITE);
        ssd1306_fillRect(cx - 6, y + 8, 3, 12, WHITE);
        hud_drawThickLine(cx - 5, y + 22, cx - 10, y + 16, WHITE);
        hud_drawThickLine(cx - 5, y + 22, cx, y + 16, WHITE);
    } else {
        // ARRIVED / Destination Pin
        ssd1306_drawCircle(cx, cy - 3, 6, WHITE);
        ssd1306_fillRect(cx - 1, cy - 4, 3, 3, WHITE);
        hud_drawThickLine(cx - 5, cy + 1, cx, y + 23, WHITE);
        hud_drawThickLine(cx + 5, cy + 1, cx, y + 23, WHITE);
    }
}

// Draw battery icon (14x7) at (x, y) with fill percentage
static inline void hud_drawBattery(int x, int y, int pct)
{
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    ssd1306_drawRect(x, y, 12, 7, WHITE);
    ssd1306_fillRect(x + 12, y + 2, 2, 3, WHITE); // positive terminal nub
    int fill = (pct * 8) / 100;
    if (fill > 0) {
        ssd1306_fillRect(x + 2, y + 2, fill, 3, WHITE);
    }
}

#endif /* HUD_ICONS_H_ */
