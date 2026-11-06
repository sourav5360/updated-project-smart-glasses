#ifndef HDMI_PRISM_HUD_H_
#define HDMI_PRISM_HUD_H_

#include <stdint.h>

/*
 * 0.23" HDMI Micro-OLED Optical Prism Driver (via Linux Framebuffer /dev/fb0)
 * Supports 640x400, 800x600, 1024x768, or 1280x720 HDMI Micro-OLED modules
 * connected to the Raspberry Pi Zero W / Zero 2 W Mini-HDMI port.
 *
 * Uses a dark transparent-style AR theme (Pure Black = 0x00000000 = 100% transparent
 * inside an optical prism combiner!) with high-contrast Cyan, Emerald Green, and Amber HUD elements.
 */

// 32-bit ARGB8888 Colors (Black pixels are completely see-through in a Micro-OLED Prism!)
#define ARGB_TRANSPARENT_BLACK 0xFF000000
#define ARGB_HUD_CYAN          0xFF00F0FF
#define ARGB_HUD_WHITE         0xFFFFFFFF
#define ARGB_WHATSAPP_GREEN    0xFF25D366
#define ARGB_MAPS_BLUE         0xFF4285F4
#define ARGB_SPOTIFY_GREEN     0xFF1DB954
#define ARGB_ALERT_RED         0xFFFF3B30
#define ARGB_AMBER_GOLD        0xFFFFCC00
#define ARGB_DARK_CARD_BG      0xFF101820

int  hdmi_prism_begin(const char *fb_device);
void hdmi_prism_setMirror(int mirror_x, int mirror_y);
void hdmi_prism_clear(uint32_t argb_color);
void hdmi_prism_display(void);

void hdmi_prism_drawPixel(int x, int y, uint32_t argb);
void hdmi_prism_fillRect(int x, int y, int w, int h, uint32_t argb);
void hdmi_prism_drawRect(int x, int y, int w, int h, int thickness, uint32_t argb);
void hdmi_prism_drawProgressBar(int x, int y, int w, int h, int pct, uint32_t fg_argb, uint32_t border_argb);
void hdmi_prism_drawStringAt(int x, int y, const char *str, int scale, uint32_t fg_argb);
void hdmi_prism_drawBitmap1Bit(int x, int y, const unsigned char *bitmap, int w, int h, int scale, uint32_t fg_argb);

#endif /* HDMI_PRISM_HUD_H_ */
