package com.smartglasses.companion;

import android.graphics.Bitmap;
import android.graphics.Color;

/**
 * Converts any Android Bitmap (photo, QR code, camera frame, or screen capture)
 * into a 128x64 1-bit monochrome image using Floyd-Steinberg error-diffusion dithering,
 * and encodes it as a 2048-character hex string for the Smart Glasses BMP|<hex> protocol.
 */
public class BitmapProjector {
    public static final int OLED_WIDTH = 128;
    public static final int OLED_HEIGHT = 64;

    public static String bitmapToMonochromeHex(Bitmap source) {
        if (source == null) return null;
        Bitmap scaled = Bitmap.createScaledBitmap(source, OLED_WIDTH, OLED_HEIGHT, true);

        float[][] lum = new float[OLED_HEIGHT][OLED_WIDTH];
        for (int y = 0; y < OLED_HEIGHT; y++) {
            for (int x = 0; x < OLED_WIDTH; x++) {
                int pixel = scaled.getPixel(x, y);
                int r = Color.red(pixel);
                int g = Color.green(pixel);
                int b = Color.blue(pixel);
                lum[y][x] = 0.299f * r + 0.587f * g + 0.114f * b;
            }
        }

        // Floyd-Steinberg Dithering for crisp 1-bit OLED detail
        boolean[][] bw = new boolean[OLED_HEIGHT][OLED_WIDTH];
        for (int y = 0; y < OLED_HEIGHT; y++) {
            for (int x = 0; x < OLED_WIDTH; x++) {
                float oldVal = lum[y][x];
                boolean isWhite = oldVal >= 128.0f;
                bw[y][x] = isWhite;
                float newVal = isWhite ? 255.0f : 0.0f;
                float err = oldVal - newVal;

                if (x + 1 < OLED_WIDTH) lum[y][x + 1] += err * (7.0f / 16.0f);
                if (x - 1 >= 0 && y + 1 < OLED_HEIGHT) lum[y + 1][x - 1] += err * (3.0f / 16.0f);
                if (y + 1 < OLED_HEIGHT) lum[y + 1][x] += err * (5.0f / 16.0f);
                if (x + 1 < OLED_WIDTH && y + 1 < OLED_HEIGHT) lum[y + 1][x + 1] += err * (1.0f / 16.0f);
            }
        }

        // Pack into 1024 bytes (16 bytes per row, MSB first) -> 2048 hex chars
        StringBuilder hex = new StringBuilder(2048);
        for (int y = 0; y < OLED_HEIGHT; y++) {
            for (int byteX = 0; byteX < OLED_WIDTH / 8; byteX++) {
                int b = 0;
                for (int bit = 0; bit < 8; bit++) {
                    if (bw[y][byteX * 8 + bit]) {
                        b |= (1 << (7 - bit));
                    }
                }
                hex.append(String.format("%02X", b));
            }
        }
        return hex.toString();
    }
}
