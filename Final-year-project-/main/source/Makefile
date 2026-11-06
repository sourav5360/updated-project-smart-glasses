CC = gcc
CFLAGS = -O2 -Wall -Wextra
LIBS = -lwiringPi -lbluetooth -lpthread

TARGET = raspberry
SRCS = raspberry.c ssd1306_i2c.c st7789_spi.c hdmi_prism_hud.c smart_sensors.c

all: $(TARGET)

$(TARGET): $(SRCS) ssd1306_i2c.h st7789_spi.h hdmi_prism_hud.h smart_sensors.h hud_icons.h oled_fonts.h
	$(CC) $(CFLAGS) $(SRCS) $(LIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)
