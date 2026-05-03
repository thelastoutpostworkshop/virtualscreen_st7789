#ifndef _VIRTUAL_SCREEN_
#define _VIRTUAL_SCREEN_

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <initializer_list>
#include <string.h>
#include <vector>

#if !defined(ESP32)
#error "This sketch uses Arduino_ESP32SPI and targets ESP32 / ESP32-S2 / ESP32-S3 boards."
#endif

#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

#if defined(ESP32) && CONFIG_IDF_TARGET_ESP32
#define VIRTUAL_SCREEN_DEFAULT_SPI_PORT VSPI
#else
#define VIRTUAL_SCREEN_DEFAULT_SPI_PORT FSPI
#endif

#ifndef TFT_BLACK
#define TFT_BLACK RGB565_BLACK
#define TFT_BLUE RGB565_BLUE
#define TFT_RED RGB565_RED
#define TFT_GREEN RGB565_GREEN
#define TFT_CYAN RGB565_CYAN
#define TFT_MAGENTA RGB565_MAGENTA
#define TFT_YELLOW RGB565_YELLOW
#define TFT_WHITE RGB565_WHITE
#endif

struct ScreenRow
{
    int8_t cs;
    uint8_t rotation;
};

struct Screen
{
    int row;
    int column;
    int8_t cs;
    uint8_t rotation;
    bool dirty;
    Arduino_DataBus *bus;
    Arduino_GFX *display;
};

struct VirtualDisplayConfig
{
    int16_t panelWidth;
    int16_t panelHeight;
    int8_t dc;
    int8_t sck;
    int8_t mosi;
    int8_t miso;
    int8_t rst;
    int8_t bl;
    uint8_t spiPort;
    bool ips;
    int32_t spiSpeed;
    uint8_t colOffset1;
    uint8_t rowOffset1;
    uint8_t colOffset2;
    uint8_t rowOffset2;

    VirtualDisplayConfig(
        int16_t panelWidth,
        int16_t panelHeight,
        int8_t dc,
        int8_t sck,
        int8_t mosi,
        int8_t miso = GFX_NOT_DEFINED,
        int8_t rst = GFX_NOT_DEFINED,
        int8_t bl = GFX_NOT_DEFINED,
        uint8_t spiPort = VIRTUAL_SCREEN_DEFAULT_SPI_PORT,
        bool ips = true,
        int32_t spiSpeed = 40000000,
        uint8_t colOffset1 = 0,
        uint8_t rowOffset1 = 0,
        uint8_t colOffset2 = 0,
        uint8_t rowOffset2 = 0)
        : panelWidth(panelWidth),
          panelHeight(panelHeight),
          dc(dc),
          sck(sck),
          mosi(mosi),
          miso(miso),
          rst(rst),
          bl(bl),
          spiPort(spiPort),
          ips(ips),
          spiSpeed(spiSpeed),
          colOffset1(colOffset1),
          rowOffset1(rowOffset1),
          colOffset2(colOffset2),
          rowOffset2(rowOffset2)
    {
    }
};

class ScreenBuilder
{
private:
    std::vector<Screen> screens;
    int totalRows = 0;
    int maxColumns = 0;
    int virtualScreenWidth = 0;
    int virtualScreenHeight = 0;
    int16_t panelWidth;
    int16_t panelHeight;

public:
    ScreenBuilder(int16_t panelWidth, int16_t panelHeight)
        : panelWidth(panelWidth), panelHeight(panelHeight)
    {
    }

    ScreenBuilder &addRow(std::initializer_list<ScreenRow> screenRows)
    {
        int columnCount = (int)screenRows.size();
        if (columnCount > maxColumns)
        {
            maxColumns = columnCount;
        }

        int column = 0;
        for (const auto &screenRow : screenRows)
        {
            screens.push_back({totalRows, column, screenRow.cs, screenRow.rotation, false, nullptr, nullptr});
            ++column;
        }
        ++totalRows;

        virtualScreenWidth = panelWidth * maxColumns;
        virtualScreenHeight = panelHeight * totalRows;

        return *this;
    }

    std::vector<Screen> &getScreens()
    {
        return screens;
    }

    const std::vector<Screen> &getScreens() const
    {
        return screens;
    }

    Screen *getScreenByGrid(int row, int column)
    {
        for (auto &screen : screens)
        {
            if (screen.row == row && screen.column == column)
            {
                return &screen;
            }
        }

        return nullptr;
    }

    Screen *getScreen(int16_t x, int16_t y)
    {
        int column = x / panelWidth;
        int row = y / panelHeight;
        return getScreenByGrid(row, column);
    }

    int width() const
    {
        return virtualScreenWidth;
    }

    int height() const
    {
        return virtualScreenHeight;
    }

    int16_t physicalWidth() const
    {
        return panelWidth;
    }

    int16_t physicalHeight() const
    {
        return panelHeight;
    }
};

class VirtualDisplay : public Arduino_GFX
{
private:
    VirtualDisplayConfig config;
    ScreenBuilder *screenBuilder;
    uint16_t *canvas = nullptr;
    uint16_t *displayBuffer = nullptr;
    size_t canvasPixels = 0;
    size_t displayBufferPixels = 0;
    bool memoryReady = false;
    bool physicalReady = false;

    uint16_t *allocatePixelBuffer(size_t pixels, const char *name)
    {
        size_t bytes = pixels * sizeof(uint16_t);
        void *buffer = nullptr;

#if defined(ESP32)
        if (psramFound())
        {
            buffer = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }
#endif

        if (!buffer)
        {
            buffer = malloc(bytes);
        }

        if (!buffer)
        {
            Serial.printf(">>> Unable to allocate %s (%u bytes)\n", name, (unsigned int)bytes);
            return nullptr;
        }

        memset(buffer, 0, bytes);
        return (uint16_t *)buffer;
    }

    void resetSharedDisplayPin()
    {
        if (config.rst == GFX_NOT_DEFINED)
        {
            return;
        }

        pinMode(config.rst, OUTPUT);
        digitalWrite(config.rst, HIGH);
        delay(20);
        digitalWrite(config.rst, LOW);
        delay(120);
        digitalWrite(config.rst, HIGH);
        delay(120);
    }

    void enableBacklight()
    {
        if (config.bl == GFX_NOT_DEFINED)
        {
            return;
        }

        pinMode(config.bl, OUTPUT);
        digitalWrite(config.bl, HIGH);
    }

    bool initPhysicalScreens(int32_t speed)
    {
        auto &screens = screenBuilder->getScreens();

        for (auto &screen : screens)
        {
            pinMode(screen.cs, OUTPUT);
            digitalWrite(screen.cs, HIGH);
        }

        resetSharedDisplayPin();
        enableBacklight();

        for (auto &screen : screens)
        {
            screen.bus = new Arduino_ESP32SPI(
                config.dc,
                screen.cs,
                config.sck,
                config.mosi,
                config.miso,
                config.spiPort,
                true);

            screen.display = new Arduino_ST7789(
                screen.bus,
                GFX_NOT_DEFINED,
                screen.rotation,
                config.ips,
                config.panelWidth,
                config.panelHeight,
                config.colOffset1,
                config.rowOffset1,
                config.colOffset2,
                config.rowOffset2);

            if (!screen.bus || !screen.display)
            {
                Serial.printf(">>> Unable to allocate display object for CS %d\n", screen.cs);
                return false;
            }

            if (!screen.display->begin(speed))
            {
                Serial.printf(">>> Display begin failed for CS %d\n", screen.cs);
                return false;
            }

            screen.display->fillScreen(RGB565_BLACK);
            screen.dirty = false;
        }

        return true;
    }

    uint16_t *getScreenImage(const Screen &screen)
    {
        size_t virtualWidth = (size_t)width();
        size_t sourceX = (size_t)screen.column * config.panelWidth;
        size_t sourceY = (size_t)screen.row * config.panelHeight;

        for (int16_t y = 0; y < config.panelHeight; ++y)
        {
            uint16_t *source = canvas + ((sourceY + y) * virtualWidth) + sourceX;
            uint16_t *target = displayBuffer + ((size_t)y * config.panelWidth);
            memcpy(target, source, (size_t)config.panelWidth * sizeof(uint16_t));
        }

        return displayBuffer;
    }

    void markDirtyRegion(int16_t x, int16_t y, int16_t w, int16_t h)
    {
        if (w <= 0 || h <= 0)
        {
            return;
        }

        int16_t x2 = x + w - 1;
        int16_t y2 = y + h - 1;

        if (x < 0)
        {
            x = 0;
        }
        if (y < 0)
        {
            y = 0;
        }
        if (x2 >= width())
        {
            x2 = width() - 1;
        }
        if (y2 >= height())
        {
            y2 = height() - 1;
        }

        int firstColumn = x / config.panelWidth;
        int lastColumn = x2 / config.panelWidth;
        int firstRow = y / config.panelHeight;
        int lastRow = y2 / config.panelHeight;

        for (int row = firstRow; row <= lastRow; ++row)
        {
            for (int column = firstColumn; column <= lastColumn; ++column)
            {
                Screen *screen = screenBuilder->getScreenByGrid(row, column);
                if (screen)
                {
                    screen->dirty = true;
                }
            }
        }
    }

    uint16_t adjustBrightness(uint16_t color, float intensity)
    {
        uint8_t r = (color >> 11) & 0x1F;
        uint8_t g = (color >> 5) & 0x3F;
        uint8_t b = color & 0x1F;

        float adjustedR = r * (1.0f + intensity);
        float adjustedG = g * (1.0f + intensity);
        float adjustedB = b * (1.0f + intensity);

        r = adjustedR > 31.0f ? 31 : (uint8_t)adjustedR;
        g = adjustedG > 63.0f ? 63 : (uint8_t)adjustedG;
        b = adjustedB > 31.0f ? 31 : (uint8_t)adjustedB;

        return (r << 11) | (g << 5) | b;
    }

public:
    VirtualDisplay(const VirtualDisplayConfig &config, ScreenBuilder *builder)
        : Arduino_GFX(builder->width(), builder->height()),
          config(config),
          screenBuilder(builder)
    {
        canvasPixels = (size_t)width() * height();
        displayBufferPixels = (size_t)config.panelWidth * config.panelHeight;

        canvas = allocatePixelBuffer(canvasPixels, "virtual canvas");
        displayBuffer = allocatePixelBuffer(displayBufferPixels, "display buffer");
        memoryReady = canvas && displayBuffer;

        if (!memoryReady)
        {
            Serial.println(">>> Not enough memory for virtual screen");
            Serial.println(">>> Enable PSRAM in the board configuration if supported");
        }
    }

    ~VirtualDisplay()
    {
        if (canvas)
        {
            free(canvas);
        }
        if (displayBuffer)
        {
            free(displayBuffer);
        }
    }

    bool begin(int32_t speed = GFX_NOT_DEFINED) override
    {
        if (!memoryReady)
        {
            return false;
        }

        if (speed == GFX_NOT_DEFINED)
        {
            speed = config.spiSpeed;
        }

        physicalReady = initPhysicalScreens(speed);
        if (physicalReady)
        {
            Serial.printf("Virtual Screen Width=%d\n", width());
            Serial.printf("Virtual Screen Height=%d\n", height());
            Serial.printf("Physical panel=%dx%d\n", config.panelWidth, config.panelHeight);
#if defined(ESP32)
            Serial.printf("PSRAM=%s\n", psramFound() ? "yes" : "no");
#endif
        }

        return physicalReady;
    }

    void output(bool force = false)
    {
        if (!physicalReady)
        {
            return;
        }

        auto &screens = screenBuilder->getScreens();
        for (auto &screen : screens)
        {
            if (!force && !screen.dirty)
            {
                continue;
            }

            uint16_t *screenImage = getScreenImage(screen);
            screen.display->draw16bitRGBBitmap(0, 0, screenImage, config.panelWidth, config.panelHeight);
            screen.display->flush();
            screen.dirty = false;
        }
    }

    void flush(bool force_flush = false) override
    {
        output(force_flush);
    }

    void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override
    {
        if (!memoryReady || x < 0 || y < 0 || x >= width() || y >= height())
        {
            return;
        }

        canvas[(size_t)y * width() + x] = color;
        markDirtyRegion(x, y, 1, 1);
    }

    void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override
    {
        if (!memoryReady || w <= 0 || h <= 0)
        {
            return;
        }

        if (x < 0)
        {
            w += x;
            x = 0;
        }
        if (y < 0)
        {
            h += y;
            y = 0;
        }
        if (x >= width() || y >= height() || w <= 0 || h <= 0)
        {
            return;
        }
        if (x + w > width())
        {
            w = width() - x;
        }
        if (y + h > height())
        {
            h = height() - y;
        }

        for (int16_t row = y; row < y + h; ++row)
        {
            uint16_t *target = canvas + ((size_t)row * width()) + x;
            for (int16_t col = 0; col < w; ++col)
            {
                target[col] = color;
            }
        }

        markDirtyRegion(x, y, w, h);
    }

    void drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h)
    {
        draw16bitRGBBitmap(x, y, bitmap, w, h);
    }

    void drawRGBBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h)
    {
        draw16bitRGBBitmap(x, y, bitmap, w, h);
    }

    void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t *buffer)
    {
        draw16bitRGBBitmap(x, y, buffer, w, h);
    }

    void readRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t *buffer)
    {
        if (!memoryReady || !buffer || w <= 0 || h <= 0)
        {
            return;
        }

        for (int16_t row = 0; row < h; ++row)
        {
            for (int16_t col = 0; col < w; ++col)
            {
                int16_t sourceX = x + col;
                int16_t sourceY = y + row;
                size_t bufferIndex = (size_t)row * w + col;

                if (sourceX >= 0 && sourceX < width() && sourceY >= 0 && sourceY < height())
                {
                    buffer[bufferIndex] = canvas[(size_t)sourceY * width() + sourceX];
                }
                else
                {
                    buffer[bufferIndex] = RGB565_BLACK;
                }
            }
        }
    }

    void highlightArea(int16_t x, int16_t y, int16_t w, int16_t h, float intensity)
    {
        if (!memoryReady)
        {
            return;
        }

        for (int16_t row = y; row < y + h; ++row)
        {
            for (int16_t col = x; col < x + w; ++col)
            {
                if (col >= 0 && col < width() && row >= 0 && row < height())
                {
                    uint16_t originalColor = canvas[(size_t)row * width() + col];
                    drawPixel(col, row, adjustBrightness(originalColor, intensity));
                }
            }
        }
    }

    void highlightArea(int16_t centerX, int16_t centerY, int16_t radius, float intensity)
    {
        if (!memoryReady)
        {
            return;
        }

        for (int16_t y = centerY - radius; y <= centerY + radius; ++y)
        {
            for (int16_t x = centerX - radius; x <= centerX + radius; ++x)
            {
                int16_t dx = x - centerX;
                int16_t dy = y - centerY;
                if ((dx * dx + dy * dy) <= (radius * radius) && x >= 0 && x < width() && y >= 0 && y < height())
                {
                    uint16_t originalColor = canvas[(size_t)y * width() + x];
                    drawPixel(x, y, adjustBrightness(originalColor, intensity));
                }
            }
        }
    }
};

#endif
