/*
 * 1.3" ST7789 IPS (240x240, 65K Color RGB565, SPI) Driver for Raspberry Pi Zero W / Zero 2 W
 * Designed to coexist with the Waveshare WM8960 Audio HAT with ZERO GPIO pin conflicts:
 *
 * Pinout (BCM GPIO / Physical Pin):
 *   VCC  -> 3.3V      (Physical Pin 1)
 *   GND  -> GND       (Physical Pin 6)
 *   SCLK -> GPIO 11   (Physical Pin 23 - SPI0 SCLK)
 *   MOSI -> GPIO 10   (Physical Pin 19 - SPI0 MOSI)
 *   CS   -> GPIO 8    (Physical Pin 24 - SPI0 CE0)
 *   RST  -> GPIO 27   (Physical Pin 13)
 *   DC   -> GPIO 25   (Physical Pin 22)
 *   BL   -> GPIO 24   (Physical Pin 18 - Backlight)
 *   (Note: Avoids GPIO 18/19/20/21 which are used by WM8960 I2S Audio!)
 */

#include "st7789_spi.h"
#include "oled_fonts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

// BCM GPIO pins (chosen specifically so they do NOT conflict with WM8960 I2S or I2C)
#define ST7789_PIN_RST 27
#define ST7789_PIN_DC  25
#define ST7789_PIN_BL  24

// Full 240x240 16-bit RGB565 Framebuffer (115,200 bytes)
static uint16_t fb_rgb565[ST7789_WIDTH * ST7789_HEIGHT];
static int spi_fd = -1;

// Sysfs GPIO helper so no external library is required beyond standard Linux kernel
static void gpio_export_and_dir(int pin)
{
    char path[64], buf[16];
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd >= 0) {
        int n = snprintf(buf, sizeof(buf), "%d", pin);
        write(fd, buf, n);
        close(fd);
    }
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    fd = open(path, O_WRONLY);
    if (fd >= 0) {
        write(fd, "out", 3);
        close(fd);
    }
}

static void gpio_write_pin(int pin, int val)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        write(fd, val ? "1" : "0", 1);
        close(fd);
    }
}

static void st7789_cmd(uint8_t cmd)
{
    if (spi_fd < 0) return;
    gpio_write_pin(ST7789_PIN_DC, 0);
    write(spi_fd, &cmd, 1);
}

static void st7789_data(uint8_t data)
{
    if (spi_fd < 0) return;
    gpio_write_pin(ST7789_PIN_DC, 1);
    write(spi_fd, &data, 1);
}

int st7789_begin(void)
{
    gpio_export_and_dir(ST7789_PIN_RST);
    gpio_export_and_dir(ST7789_PIN_DC);
    gpio_export_and_dir(ST7789_PIN_BL);

    gpio_write_pin(ST7789_PIN_BL, 1);

    spi_fd = open("/dev/spidev0.0", O_RDWR);
    if (spi_fd < 0) {
        fprintf(stderr, "[ST7789] /dev/spidev0.0 not open (enable SPI via raspi-config)\n");
        return -1;
    }

    // Hardware reset pulse
    gpio_write_pin(ST7789_PIN_RST, 1);
    usleep(50000);
    gpio_write_pin(ST7789_PIN_RST, 0);
    usleep(50000);
    gpio_write_pin(ST7789_PIN_RST, 1);
    usleep(150000);

    st7789_cmd(0x01); // SWRESET
    usleep(150000);
    st7789_cmd(0x11); // SLPOUT (Sleep Out)
    usleep(120000);

    st7789_cmd(0x3A); // COLMOD: 16-bit RGB565 (0x55)
    st7789_data(0x55);

    st7789_cmd(0x36); // MADCTL: Memory Data Access Control
    st7789_data(0x00);

    st7789_cmd(0x21); // INVON: IPS panel color inversion ON
    st7789_cmd(0x13); // NORON: Normal display mode
    usleep(10000);
    st7789_cmd(0x29); // DISPON: Display ON

    st7789_clear(COLOR_BLACK);
    st7789_display();
    return 0;
}

// Configure hardware mirror (MADCTL 0x36) for Smart Glasses 45-deg combiner prism
void st7789_setMirror(int mirror_x, int mirror_y)
{
    uint8_t madctl = 0x00;
    if (mirror_x) madctl |= 0x40; // MX: Column Address Order (Horizontal Flip)
    if (mirror_y) madctl |= 0x80; // MY: Row Address Order (Vertical Flip)
    st7789_cmd(0x36);
    st7789_data(madctl);
}

void st7789_setBacklight(int on)
{
    gpio_write_pin(ST7789_PIN_BL, on ? 1 : 0);
}

void st7789_clear(uint16_t color)
{
    int i;
    for (i = 0; i < ST7789_WIDTH * ST7789_HEIGHT; i++) {
        fb_rgb565[i] = color;
    }
}

void st7789_display(void)
{
    if (spi_fd < 0) return;

    // Set column address 0..239
    st7789_cmd(0x2A);
    st7789_data(0x00); st7789_data(0x00);
    st7789_data(0x00); st7789_data(ST7789_WIDTH - 1);

    // Set row address 0..239
    st7789_cmd(0x2B);
    st7789_data(0x00); st7789_data(0x00);
    st7789_data(0x00); st7789_data(ST7789_HEIGHT - 1);

    // Write RAM
    st7789_cmd(0x2C);
    gpio_write_pin(ST7789_PIN_DC, 1);

    // Convert to big-endian RGB565 and write in 4096-byte SPI DMA chunks
    uint8_t chunk[4096];
    int total_pixels = ST7789_WIDTH * ST7789_HEIGHT;
    int idx = 0;
    while (idx < total_pixels) {
        int bytes = 0;
        while (bytes < (int)sizeof(chunk) && idx < total_pixels) {
            uint16_t px = fb_rgb565[idx++];
            chunk[bytes++] = (uint8_t)(px >> 8);
            chunk[bytes++] = (uint8_t)(px & 0xFF);
        }
        write(spi_fd, chunk, bytes);
    }
}

void st7789_drawPixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= ST7789_WIDTH || y < 0 || y >= ST7789_HEIGHT) return;
    fb_rgb565[y * ST7789_WIDTH + x] = color;
}

void st7789_fillRect(int x, int y, int w, int h, uint16_t color)
{
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ST7789_WIDTH) w = ST7789_WIDTH - x;
    if (y + h > ST7789_HEIGHT) h = ST7789_HEIGHT - y;
    int row, col;
    for (row = y; row < y + h; row++) {
        for (col = x; col < x + w; col++) {
            fb_rgb565[row * ST7789_WIDTH + col] = color;
        }
    }
}

void st7789_drawRect(int x, int y, int w, int h, uint16_t color)
{
    st7789_fillRect(x, y, w, 1, color);
    st7789_fillRect(x, y + h - 1, w, 1, color);
    st7789_fillRect(x, y, 1, h, color);
    st7789_fillRect(x + w - 1, y, 1, h, color);
}

void st7789_drawLine(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (1) {
        st7789_drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void st7789_drawCircle(int x0, int y0, int r, uint16_t color)
{
    int f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
    st7789_drawPixel(x0, y0 + r, color);
    st7789_drawPixel(x0, y0 - r, color);
    st7789_drawPixel(x0 + r, y0, color);
    st7789_drawPixel(x0 - r, y0, color);
    while (x < y) {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++; ddF_x += 2; f += ddF_x;
        st7789_drawPixel(x0 + x, y0 + y, color);
        st7789_drawPixel(x0 - x, y0 + y, color);
        st7789_drawPixel(x0 + x, y0 - y, color);
        st7789_drawPixel(x0 - x, y0 - y, color);
        st7789_drawPixel(x0 + y, y0 + x, color);
        st7789_drawPixel(x0 - y, y0 + x, color);
        st7789_drawPixel(x0 + y, y0 - x, color);
        st7789_drawPixel(x0 - y, y0 - x, color);
    }
}

void st7789_drawProgressBar(int x, int y, int w, int h, int percentage, uint16_t fgColor, uint16_t bgColor)
{
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;
    st7789_fillRect(x, y, w, h, bgColor);
    st7789_drawRect(x, y, w, h, fgColor);
    int fill_w = ((w - 4) * percentage) / 100;
    if (fill_w > 0 && h > 4) {
        st7789_fillRect(x + 2, y + 2, fill_w, h - 4, fgColor);
    }
}

void st7789_drawChar(int x, int y, unsigned char c, uint16_t fgColor, int size)
{
    if (c > 127) c = '?';
    int i, j;
    for (i = 0; i < 6; i++) {
        uint8_t line = (i == 5) ? 0x00 : font[(c * 5) + i];
        for (j = 0; j < 8; j++) {
            if (line & 0x1) {
                if (size <= 1) st7789_drawPixel(x + i, y + j, fgColor);
                else st7789_fillRect(x + i * size, y + j * size, size, size, fgColor);
            }
            line >>= 1;
        }
    }
}

void st7789_drawStringAt(int x, int y, const char *str, int size, uint16_t fgColor)
{
    if (!str) return;
    int s = (size > 0) ? size : 1;
    int cx = x, cy = y;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            cy += s * 10;
            cx = x;
        } else {
            st7789_drawChar(cx, cy, (unsigned char)str[i], fgColor, s);
            cx += s * 6;
            if (cx > ST7789_WIDTH - s * 6) {
                cy += s * 10;
                cx = x;
            }
        }
    }
}

void st7789_drawBitmap1Bit(int x, int y, const unsigned char *bitmap, int w, int h, uint16_t fgColor)
{
    if (!bitmap) return;
    int byteWidth = (w + 7) / 8;
    uint8_t b = 0;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (i & 7) b <<= 1;
            else b = bitmap[j * byteWidth + i / 8];
            if (b & 0x80) st7789_drawPixel(x + i, y + j, fgColor);
        }
    }
}

void st7789_drawRGB565Frame(const uint16_t *rgb565_frame, int w, int h)
{
    if (!rgb565_frame) return;
    for (int y = 0; y < h && y < ST7789_HEIGHT; y++) {
        for (int x = 0; x < w && x < ST7789_WIDTH; x++) {
            fb_rgb565[y * ST7789_WIDTH + x] = rgb565_frame[y * w + x];
        }
    }
}
