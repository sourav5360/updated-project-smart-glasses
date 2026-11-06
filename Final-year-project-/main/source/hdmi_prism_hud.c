/*
 * 0.23" HDMI Micro-OLED Optical Prism HUD Renderer (Linux /dev/fb0)
 * Renders a 640x400 full-color AR Heads-Up Display onto any HDMI Micro-OLED Prism
 * plugged into the Raspberry Pi Mini-HDMI port.
 */

#include "hdmi_prism_hud.h"
#include "oled_fonts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define HUD_VIRTUAL_W 640
#define HUD_VIRTUAL_H 400

static uint32_t vfb[HUD_VIRTUAL_W * HUD_VIRTUAL_H];
static int fb_fd = -1;
static int g_mirror_x = 0;
static int g_mirror_y = 0;

int hdmi_prism_begin(const char *fb_device)
{
    const char *dev = fb_device ? fb_device : "/dev/fb0";
    fb_fd = open(dev, O_RDWR);
    if (fb_fd < 0) {
        return -1;
    }
    hdmi_prism_clear(ARGB_TRANSPARENT_BLACK);
    return 0;
}

void hdmi_prism_setMirror(int mirror_x, int mirror_y)
{
    g_mirror_x = mirror_x ? 1 : 0;
    g_mirror_y = mirror_y ? 1 : 0;
}

void hdmi_prism_clear(uint32_t argb_color)
{
    for (int i = 0; i < HUD_VIRTUAL_W * HUD_VIRTUAL_H; i++) {
        vfb[i] = argb_color;
    }
}

void hdmi_prism_drawPixel(int x, int y, uint32_t argb)
{
    if (x < 0 || x >= HUD_VIRTUAL_W || y < 0 || y >= HUD_VIRTUAL_H) return;
    int tx = g_mirror_x ? (HUD_VIRTUAL_W - 1 - x) : x;
    int ty = g_mirror_y ? (HUD_VIRTUAL_H - 1 - y) : y;
    vfb[ty * HUD_VIRTUAL_W + tx] = argb;
}

void hdmi_prism_fillRect(int x, int y, int w, int h, uint32_t argb)
{
    for (int row = y; row < y + h; row++) {
        for (int col = x; col < x + w; col++) {
            hdmi_prism_drawPixel(col, row, argb);
        }
    }
}

void hdmi_prism_drawRect(int x, int y, int w, int h, int thickness, uint32_t argb)
{
    if (thickness < 1) thickness = 1;
    hdmi_prism_fillRect(x, y, w, thickness, argb);
    hdmi_prism_fillRect(x, y + h - thickness, w, thickness, argb);
    hdmi_prism_fillRect(x, y, thickness, h, argb);
    hdmi_prism_fillRect(x + w - thickness, y, thickness, h, argb);
}

void hdmi_prism_drawProgressBar(int x, int y, int w, int h, int pct, uint32_t fg_argb, uint32_t border_argb)
{
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    hdmi_prism_drawRect(x, y, w, h, 2, border_argb);
    int fill_w = ((w - 8) * pct) / 100;
    if (fill_w > 0 && h > 8) {
        hdmi_prism_fillRect(x + 4, y + 4, fill_w, h - 8, fg_argb);
    }
}

void hdmi_prism_drawStringAt(int x, int y, const char *str, int scale, uint32_t fg_argb)
{
    if (!str) return;
    int s = (scale > 0) ? scale : 2;
    int cx = x, cy = y;
    for (size_t idx = 0; str[idx] != '\0'; idx++) {
        unsigned char c = (unsigned char)str[idx];
        if (c == '\n') {
            cy += s * 10;
            cx = x;
            continue;
        }
        if (c > 127) c = '?';
        for (int col = 0; col < 5; col++) {
            uint8_t line = font[c * 5 + col];
            for (int row = 0; row < 8; row++) {
                if (line & (1 << row)) {
                    hdmi_prism_fillRect(cx + col * s, cy + row * s, s, s, fg_argb);
                }
            }
        }
        cx += s * 6;
        if (cx > HUD_VIRTUAL_W - s * 6) {
            cy += s * 10;
            cx = x;
        }
    }
}

void hdmi_prism_drawBitmap1Bit(int x, int y, const unsigned char *bitmap, int w, int h, int scale, uint32_t fg_argb)
{
    if (!bitmap) return;
    int s = (scale > 0) ? scale : 2;
    int byteWidth = (w + 7) / 8;
    uint8_t b = 0;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (i & 7) b <<= 1;
            else b = bitmap[j * byteWidth + i / 8];
            if (b & 0x80) {
                hdmi_prism_fillRect(x + i * s, y + j * s, s, s, fg_argb);
            }
        }
    }
}

void hdmi_prism_display(void)
{
    if (fb_fd < 0) return;
    lseek(fb_fd, 0, SEEK_SET);
    write(fb_fd, vfb, sizeof(vfb));
}
