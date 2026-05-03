# Virtual Screen ST7789

Arduino sketch for driving five 240x240 ST7789 SPI TFT displays as one wide virtual screen. The sketch uses one shared SPI bus and one framebuffer, then sends each 240x240 section to the correct physical display.

## Hardware

Use an ESP32-S3 board with PSRAM. Five 240x240 RGB565 displays require a large framebuffer:

- Virtual framebuffer: `1200 x 240 x 2` bytes, about 576 KB
- Transfer buffer: `240 x 240 x 2` bytes, about 115 KB

An ESP32 without PSRAM is not suitable for this sketch. An ESP32-S3 N8R8 or similar board with 8 MB PSRAM is recommended.

## Arduino IDE Configuration

Install these board and library dependencies:

- Board package: `esp32` by Espressif Systems
- Library: `GFX Library for Arduino` by Moon On Our Nation

Recommended board settings:

- Board: `ESP32S3 Dev Module`
- PSRAM: `OPI PSRAM` for common ESP32-S3 N8R8 boards
- CPU Frequency: `240MHz`
- Flash Mode: `QIO 80MHz`
- Upload Speed: `921600`


## Display Wiring

All five displays share the same SPI and control pins, except each display has its own chip-select pin.

Default shared pin configuration in [virtualscreen_st7789.ino](virtualscreen_st7789.ino):

| Display pin | ESP32-S3 GPIO | Sketch setting |
| --- | ---: | --- |
| SCL / SCK / CLK | 36 | `TFT_SCK` |
| SDA / MOSI / DIN | 35 | `TFT_MOSI` |
| DC / A0 | 41 | `TFT_DC` |
| RST / RESET | 42 | `TFT_RST` |
| BL / BLK / LED | 3V3 or GPIO | `TFT_BL` |
| VCC | 3V3 | n/a |
| GND | GND | n/a |

Default chip-select pins:

| Screen | ESP32-S3 GPIO | Sketch entry |
| --- | ---: | --- |
| 1 | 25 | `{25, 0}` |
| 2 | 12 | `{12, 0}` |
| 3 | 17 | `{17, 0}` |
| 4 | 13 | `{13, 0}` |
| 5 | 26 | `{26, 0}` |

The screen order is set in `configureScreens()`:

```cpp
screens.addRow({{25, 0}, {12, 0}, {17, 0}, {13, 0}, {26, 0}});
```

The second value is display rotation. Use `0`, `1`, `2`, or `3` if a screen is mounted in a different orientation.

## Changing Pins

Edit the constants near the top of [virtualscreen_st7789.ino](virtualscreen_st7789.ino):

```cpp
static constexpr int8_t TFT_SCK = 36;
static constexpr int8_t TFT_MOSI = 35;
static constexpr int8_t TFT_DC = 41;
static constexpr int8_t TFT_RST = 42;
static constexpr int8_t TFT_BL = GFX_NOT_DEFINED;
```

Then edit the CS pins in `configureScreens()`:

```cpp
screens.addRow({{25, 0}, {12, 0}, {17, 0}, {13, 0}, {26, 0}});
```

If your display backlight is connected directly to 3V3, leave `TFT_BL` as `GFX_NOT_DEFINED`. If it is connected to a GPIO, set `TFT_BL` to that GPIO.

## ST7789 Offsets

Many 1.3 inch, 1.5 inch, and 1.54 inch square ST7789 modules need this offset:

```cpp
static constexpr uint8_t TFT_ROW_OFFSET_2 = 80;
```

If the image is shifted, cropped, or wrapped around, adjust these four values:

```cpp
static constexpr uint8_t TFT_COL_OFFSET_1 = 0;
static constexpr uint8_t TFT_ROW_OFFSET_1 = 0;
static constexpr uint8_t TFT_COL_OFFSET_2 = 0;
static constexpr uint8_t TFT_ROW_OFFSET_2 = 80;
```

## Power Notes

Five TFT displays can draw more current than a board regulator can comfortably supply, especially with backlights on. If the board resets, screens flicker, or USB disconnects, use a separate regulated 3.3 V supply for the displays and connect the supply ground to the ESP32 ground.

Do not connect display signal pins to 5 V.

## Sketch Behavior

On boot, the sketch initializes all five displays, draws a startup screen, then scrolls text across the full `1200 x 240` virtual display. Drawing happens against `VirtualDisplay`, so normal Arduino_GFX drawing functions can span across display boundaries.
