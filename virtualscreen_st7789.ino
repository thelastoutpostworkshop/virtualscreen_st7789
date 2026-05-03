#include "virtualScreen.h"

/*
  Five ST7789 panels as one wide virtual screen.

  Library required from Arduino Library Manager:
  - GFX Library for Arduino by Moon On Our Nation

  Board setup:
  - Use an ESP32-S3 with PSRAM.
  - In Arduino IDE Tools, enable PSRAM. OPI PSRAM is typical for ESP32-S3 N8R8 boards.

  Wiring model:
  - All panels share SCL/SCK, SDA/MOSI, DC, RST, VCC and GND.
  - Each panel has its own CS pin.
  - If your display module has BL/BLK, connect it to 3V3 or set TFT_BL below.
*/

static constexpr int16_t PANEL_WIDTH = 240;
static constexpr int16_t PANEL_HEIGHT = 240;

// Shared SPI/control pins. Change these to match your board wiring.
static constexpr int8_t TFT_SCK = 36;  // Display SCL/SCK
static constexpr int8_t TFT_MOSI = 35; // Display SDA/MOSI
static constexpr int8_t TFT_MISO = GFX_NOT_DEFINED;
static constexpr int8_t TFT_DC = 41;
static constexpr int8_t TFT_RST = 42;
static constexpr int8_t TFT_BL = GFX_NOT_DEFINED;

// ESP32 uses VSPI by default; ESP32-S2/S3/C3/C6 use FSPI in Arduino_GFX.
static constexpr uint8_t TFT_SPI_PORT = VIRTUAL_SCREEN_DEFAULT_SPI_PORT;
static constexpr int32_t TFT_SPI_SPEED = 40000000;

// 1.3"/1.5"/1.54" square ST7789 modules commonly need rowOffset2 = 80.
static constexpr uint8_t TFT_COL_OFFSET_1 = 0;
static constexpr uint8_t TFT_ROW_OFFSET_1 = 0;
static constexpr uint8_t TFT_COL_OFFSET_2 = 0;
static constexpr uint8_t TFT_ROW_OFFSET_2 = 80;

VirtualDisplayConfig displayConfig(
    PANEL_WIDTH,
    PANEL_HEIGHT,
    TFT_DC,
    TFT_SCK,
    TFT_MOSI,
    TFT_MISO,
    TFT_RST,
    TFT_BL,
    TFT_SPI_PORT,
    true,
    TFT_SPI_SPEED,
    TFT_COL_OFFSET_1,
    TFT_ROW_OFFSET_1,
    TFT_COL_OFFSET_2,
    TFT_ROW_OFFSET_2);

ScreenBuilder screens(PANEL_WIDTH, PANEL_HEIGHT);
VirtualDisplay *tft = nullptr;

const char *scrollText = "WAR IS PEACE    FREEDOM IS SLAVERY    IGNORANCE IS STRENGTH    ";
int16_t scrollX = 0;
uint16_t scrollWidth = 0;

void configureScreens()
{
    // CS pins from the issue: screen 1=25, screen 2=12, screen 3=17, screen 4=13, screen 5=26.
    screens.addRow({{25, 0}, {12, 0}, {17, 0}, {13, 0}, {26, 0}});
}

void drawPanelFrames(uint16_t color)
{
    for (int16_t x = PANEL_WIDTH; x < tft->width(); x += PANEL_WIDTH)
    {
        tft->drawFastVLine(x - 1, 0, tft->height(), color);
        tft->drawFastVLine(x, 0, tft->height(), color);
    }
}

void drawCenteredText(const char *text, int16_t y, uint8_t size, uint16_t color)
{
    int16_t x1;
    int16_t y1;
    uint16_t w;
    uint16_t h;

    tft->setFont();
    tft->setTextSize(size);
    tft->setTextColor(color);
    tft->getTextBounds(text, 0, y, &x1, &y1, &w, &h);
    tft->setCursor((tft->width() - w) / 2, y);
    tft->print(text);
}

void drawBootScreen()
{
    tft->fillScreen(RGB565_BLACK);
    drawPanelFrames(RGB565_DARKGRAY);
    tft->drawRect(4, 4, tft->width() - 8, tft->height() - 8, RGB565_DARKGRAY);
    drawCenteredText("MINISTRY OF TRUTH", 58, 8, RGB565_RED);
    drawCenteredText("ESP32-S3 + 5x ST7789 + Arduino_GFX", 154, 3, RGB565_CYAN);
    tft->output(true);
}

void setup()
{
    Serial.begin(115200);
    delay(100);

    configureScreens();

    tft = new VirtualDisplay(displayConfig, &screens);
    if (!tft || !tft->begin())
    {
        Serial.println("Virtual display init failed.");
        while (true)
        {
            delay(1000);
        }
    }

    tft->setTextWrap(false);
    drawBootScreen();
    delay(1500);

    int16_t x1;
    int16_t y1;
    uint16_t h;
    tft->setTextSize(7);
    tft->getTextBounds(scrollText, 0, 96, &x1, &y1, &scrollWidth, &h);
    scrollX = tft->width();
}

void loop()
{
    tft->fillScreen(RGB565_BLACK);
    drawPanelFrames(RGB565_DARKGRAY);

    tft->setFont();
    tft->setTextWrap(false);
    tft->setTextSize(7);
    tft->setTextColor(RGB565_YELLOW);
    tft->setCursor(scrollX, 94);
    tft->print(scrollText);

    tft->output();

    scrollX -= 8;
    if (scrollX < -((int16_t)scrollWidth))
    {
        scrollX = tft->width();
    }

    delay(30);
}
