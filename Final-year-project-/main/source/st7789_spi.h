#ifndef ST7789_SPI_H_
#define ST7789_SPI_H_

#include <stdint.h>

#define ST7789_WIDTH  240
#define ST7789_HEIGHT 240

// 16-bit RGB565 Color Definitions for Full-Color Smart Glasses HUD
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_CYAN        0x07FF
#define COLOR_GREEN       0x07E0
#define COLOR_WHATSAPP    0x2668  // WhatsApp Emerald Green
#define COLOR_YELLOW      0xFFE0
#define COLOR_ORANGE      0xFD20
#define COLOR_RED         0xF800
#define COLOR_BLUE        0x001F
#define COLOR_MAPS_BLUE   0x1C9F  // Google Maps Navigation Blue
#define COLOR_DARK_GRAY   0x2104
#define COLOR_NAVY        0x000F

// Convert 8-bit R, G, B (0-255) to 16-bit RGB565
#define RGB565(r, g, b) ((((uint16_t)(r) & 0xF8) << 8) | (((uint16_t)(g) & 0xFC) << 3) | (((uint16_t)(b) & 0xF8) >> 3))

// Initialize 1.3" ST7789 240x240 IPS SPI Display
int  st7789_begin(void);
void st7789_setMirror(int mirror_x, int mirror_y);
void st7789_setBacklight(int on);

// Framebuffer & Drawing Primitives (240x240 RGB565)
void st7789_clear(uint16_t color);
void st7789_display(void);
void st7789_drawPixel(int x, int y, uint16_t color);
void st7789_fillRect(int x, int y, int w, int h, uint16_t color);
void st7789_drawRect(int x, int y, int w, int h, uint16_t color);
void st7789_drawLine(int x0, int y0, int x1, int y1, uint16_t color);
void st7789_drawCircle(int x0, int y0, int r, uint16_t color);
void st7789_drawProgressBar(int x, int y, int w, int h, int percentage, uint16_t fgColor, uint16_t bgColor);

// Text & Icon Rendering
void st7789_drawChar(int x, int y, unsigned char c, uint16_t fgColor, int size);
void st7789_drawStringAt(int x, int y, const char *str, int size, uint16_t fgColor);
void st7789_drawBitmap1Bit(int x, int y, const unsigned char *bitmap, int w, int h, uint16_t fgColor);
void st7789_drawRGB565Frame(const uint16_t *rgb565_frame, int w, int h);

#endif /* ST7789_SPI_H_ */
