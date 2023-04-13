#include "tft_driver.h"

#include <stm32f4xx_hal.h>
#include <stm32f4xx_hal_spi.h>
#include <stm32f4xx_hal_dma.h>

#include <stdlib.h>
#include <string.h>

/******************* Pin map *******************\
 *   PA7 - SPI1 MOSI
 *   PA6 - SPI1 MISO
 *   PA5 - SPI1 SCK
 *   PA4 - LCD Select
 *
 *   PB3 - LCD Command/Data
 *   PB4 - LCD Reset
 *
\***********************************************/

/********** Useful Macros **********/
#define LCD_MOSI    GPIO_PIN_7
#define LCD_MISO    GPIO_PIN_6
#define LCD_SCK     GPIO_PIN_5
#define LCD_SS      GPIO_PIN_4

#define LCD_DC  GPIO_PIN_3
#define LCD_RST GPIO_PIN_4

#define LCD_SELECT() HAL_GPIO_WritePin(GPIOA, LCD_SS, GPIO_PIN_RESET);
#define LCD_DESELECT() HAL_GPIO_WritePin(GPIOA, LCD_SS, GPIO_PIN_SET);

#define LCD_MODE_CMD() HAL_GPIO_WritePin(GPIOB, LCD_DC, GPIO_PIN_RESET);
#define LCD_MODE_DATA() HAL_GPIO_WritePin(GPIOB, LCD_DC, GPIO_PIN_SET);

#define LCD_ASSERT_RST() HAL_GPIO_WritePin(GPIOB, LCD_RST, GPIO_PIN_RESET);
#define LCD_DEASSERT_RST() HAL_GPIO_WritePin(GPIOB, LCD_RST, GPIO_PIN_SET);

#define SPI_WAIT_NBSY() while(__HAL_SPI_GET_FLAG(&tft_lcdSPI, SPI_FLAG_BSY))

#include <Arduino.h>
#define LCD_DELAY(ms) delay((ms));

/********** LCD TFT driver initialization commands **********/
const uint8_t TFT_Init_Sequence[] = {
    0xFF, 0x3A, 0x55,
    0xFF, 0xC0, 0x0E, 0x0E,
    0xFF, 0xC1, 0x41, 0x00,
    0xFF, 0xC2, 0x55,
    0xFF, 0xC5, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xE0, 0x0F, 0x1F, 0x1C, 0x0C, 0x0F, 0x08, 0x48, 0x98, 0x37, 0x0A, 0x13, 0x04, 0x11, 0x0D, 0x00,
    0xFF, 0xE1, 0x0F, 0x32, 0x2E, 0x0B, 0x0D, 0x05, 0x47, 0x75, 0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00,
    0xFF, 0x20,
    0xFF, 0x36, 0x48,
    0xFF
};

/********** Global variables **********/
SPI_HandleTypeDef tft_lcdSPI;
DMA_HandleTypeDef tft_lcdDMA;

uint16_t tft_lcdBuffer[LCD_BUFFER_SIZE]; // We'll allocate a 16K buffer for drawing things

struct LcdOperation* tft_lcdOperations = 0;
struct LcdOperation* tft_lcdLastOp = 0;

/********** Functions **********/

/**
 * @brief LCD command
 * Sends a simple LCD command (1 byte)
 *
 * @param cmd Command
 */
void tft_lcd_cmd(uint8_t cmd) {
    LCD_SELECT();

    LCD_MODE_CMD();
    if(tft_lcdSPI.Init.DataSize == SPI_DATASIZE_8BIT) {
        uint8_t buffer[2];
        buffer[0] = 0;
        buffer[1] = cmd;
        HAL_SPI_Transmit(&tft_lcdSPI, buffer, 2, 10);
    } else {
        uint16_t buffer = cmd;
        HAL_SPI_Transmit(&tft_lcdSPI, (uint8_t*)&buffer, 1, 10);
    }
    SPI_WAIT_NBSY();

    LCD_MODE_DATA();

    LCD_DESELECT();
}

/**
 * @brief LCD command with data
 * Sends a command (1 byte) and it's parameters
 *
 * @param cmd Command
 * @param data Parameters as an array of bytes
 * @param length Length of data
 */
void tft_lcd_cmd_data(uint8_t cmd, const void* data, size_t length) {
    LCD_SELECT();

    LCD_MODE_CMD();
    if(tft_lcdSPI.Init.DataSize == SPI_DATASIZE_8BIT) {
        uint8_t buffer[2];
        buffer[0] = 0;
        buffer[1] = cmd;
        HAL_SPI_Transmit(&tft_lcdSPI, buffer, 2, 10);
    } else {
        uint16_t buffer = cmd;
        HAL_SPI_Transmit(&tft_lcdSPI, (uint8_t*)&buffer, 1, 10);
    }
    SPI_WAIT_NBSY();

    LCD_MODE_DATA();
    if(tft_lcdSPI.Init.DataSize == SPI_DATASIZE_8BIT) {
        HAL_SPI_Transmit(&tft_lcdSPI, (uint8_t*)data, length, 10);
    } else {
        HAL_SPI_Transmit(&tft_lcdSPI, (uint8_t*)data, length / 2, 10);
    }
    SPI_WAIT_NBSY();

    LCD_DESELECT();
}

void tft_dma_memmode(int incrementMemory) {
    int current = tft_lcdDMA.Init.MemInc == DMA_MINC_ENABLE;
    if(current && incrementMemory)
        return;
    if(!current && !incrementMemory)
        return;
    if(incrementMemory)
        tft_lcdDMA.Init.MemInc = DMA_MINC_ENABLE;
    else
        tft_lcdDMA.Init.MemInc = DMA_MINC_DISABLE;
    HAL_DMA_Init(&tft_lcdDMA);
}

/**
 * @brief LCD data
 * Sends data to LCD using DMA
 *
 * @param data Data
 * @param length Length of data
 */
void tft_lcd_dma(const void* data, size_t length) {
    LCD_SELECT();
    LCD_MODE_DATA();

    // Start the DMA transfer
    HAL_SPI_Transmit_DMA(&tft_lcdSPI, (uint8_t*)data, length);
}

/**
 * @brief Set LCD write window
 * Prepares the LCD for writing image data by setting
 * the start and end positions
 *
 * @param x0 Start column
 * @param y0 Start row
 * @param x1 End column
 * @param y1 End row
 */
void tft_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint16_t data[4];
    memset(data, 0, sizeof(data));

    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;
    tft_lcd_cmd_data(0x2A, data, sizeof(data));

    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;
    tft_lcd_cmd_data(0x2B, data, sizeof(data));

    tft_lcd_cmd(0x2C);
}

struct LcdOperation* tft_new_operation(LcdOperationEnum operation) {
    struct LcdOperation* lop = (struct LcdOperation*)malloc(sizeof(struct LcdOperation));

    lop->lo_op = operation;
    lop->lo_next = 0;
    lop->lo_static = 0;

    return lop;
}

void tft_submit(struct LcdOperation* op) {
    if(!tft_lcdOperations) {
        tft_lcdOperations = op;
        tft_lcdLastOp = op;
    } else {
        tft_lcdLastOp->lo_next = op;
        tft_lcdLastOp = op;
    }
}

void tft_submit_multiple(struct LcdOperation* ops, size_t count) {
    for(size_t i = 0; i < count; ++i) {
        // We cannot free these operations as they are part of an array,
        // we will assume that all arrays are declared statically.
        ops[i].lo_static = 1;
        tft_submit(&ops[i]);
    }
}

/**
 * @brief Insert operation after position
 * This function inserts a rendering operation after the
 * specified position in the render queue.
 *
 * @param position Position to insert after
 * @param op Operation to be inserted
 */
void tft_insert_after(struct LcdOperation* position, struct LcdOperation* op) {
    if(position == tft_lcdLastOp)
        tft_lcdLastOp = op;
    op->lo_next = position->lo_next;
    position->lo_next = op;
}


/********** Start of rendering functions **********/

void tft_render_op(struct LcdOperation* op);

#define LCD_ENCODE_COLOR(pos, color) {\
    size_t __pos = (pos); \
    tft_lcdBuffer[(__pos)] = (color).word; \
}

void tft_render_text(struct LcdOperation* op) {
    const size_t lineHeight = op->lo_text.font->bf_yAdvance;

    size_t xMax = 0;
    size_t x = 0;

    // Calculate line length
    for(const char* textPtr = op->lo_text.value; *textPtr; ++textPtr) {
        if(*textPtr == '\n') {
            x = 0;
        } else {
            if(*textPtr < op->lo_text.font->bf_firstChar || *textPtr > op->lo_text.font->bf_lastChar)
                continue;
            uint8_t glyphOffset = *textPtr - op->lo_text.font->bf_firstChar;
            struct BitmapFontGlyph glyph = op->lo_text.font->bf_glyphs[glyphOffset];
            x += glyph.bfg_xAdvance;
        }

        if(x > xMax)
            xMax = x;
    }

    size_t pixelsPerLine = xMax * lineHeight;
    size_t maxLines = LCD_BUFFER_SIZE / pixelsPerLine;

    size_t line = 1;
    size_t y = lineHeight;
    x = 0;

    for(const char* textPtr = op->lo_text.value; *textPtr; ++textPtr) {
        if(*textPtr == '\n') {
            // Fill to the end of the line
            for(size_t yy = y - lineHeight; yy < y; ++yy) {
                for(size_t xx = x; xx < xMax; ++xx) {
                    LCD_ENCODE_COLOR(xx + yy * xMax, op->lo_bg);
                }
            }

            // Move to the next line
            ++line;
            x = 0;

            if(line > maxLines) {
                // Submit a new text draw with remaining text
                struct LcdOperation* cont = tft_new_operation(TEXT);
                cont->lo_fg = op->lo_fg;
                cont->lo_bg = op->lo_bg;
                cont->lo_x = op->lo_x;
                cont->lo_y = op->lo_y + y;
                cont->lo_text.font = op->lo_text.font;
                cont->lo_text.value = textPtr + 1;

                tft_insert_after(op, cont);
                break;
            }
            y += lineHeight;
        } else {
            if(*textPtr < op->lo_text.font->bf_firstChar || *textPtr > op->lo_text.font->bf_lastChar)
                continue;
            uint8_t glyphOffset = *textPtr - op->lo_text.font->bf_firstChar;
            struct BitmapFontGlyph glyph = op->lo_text.font->bf_glyphs[glyphOffset];

            size_t bitmapOffset = glyph.bfg_bitmapOffset;
            uint8_t bits;
            uint8_t mask = 0;

            // Before bit-mapped area (vertically)
            for(size_t yy = y - lineHeight; yy < y + glyph.bfg_yOffset - 1; ++yy) {
                for(size_t xx = x; xx < x + glyph.bfg_xAdvance; ++xx) {
                    LCD_ENCODE_COLOR(xx + yy * xMax, op->lo_bg);
                }
            }

            // After bit-mapped area (vertically)
            for(size_t yy = y + glyph.bfg_yOffset - 1 + glyph.bfg_height; yy < y; ++yy) {
                for(size_t xx = x; xx < x + glyph.bfg_xAdvance; ++xx) {
                    LCD_ENCODE_COLOR(xx + yy * xMax, op->lo_bg);
                }
            }

            // Before bit-mapped area (horizontally)
            for (size_t xx = x; xx < x + glyph.bfg_xOffset; ++xx) {
                for(size_t yy = y + glyph.bfg_yOffset - 1; yy < y + glyph.bfg_yOffset - 1 + glyph.bfg_height; ++yy) {
                    LCD_ENCODE_COLOR(xx + yy * xMax, op->lo_bg);
                }
            }

            // After bit-mapped area (horizontally)
            for (size_t xx = x + glyph.bfg_xOffset + glyph.bfg_width; xx < x + glyph.bfg_xAdvance; ++xx) {
                for(size_t yy = y + glyph.bfg_yOffset - 1; yy < y + glyph.bfg_yOffset - 1 + glyph.bfg_height; ++yy) {
                    LCD_ENCODE_COLOR(xx + yy * xMax, op->lo_bg);
                }
            }

            for(size_t yy = 0; yy < glyph.bfg_height; ++yy) {
                for(size_t xx = 0; xx < glyph.bfg_width; ++xx) {
                    if(!mask) {
                        mask = 0x80;
                        bits = op->lo_text.font->bf_bitmap[bitmapOffset++];
                    }

                    LcdColor color = (bits & mask) ? op->lo_fg : op->lo_bg;
                    mask >>= 1;

                    size_t xPos = x + xx + glyph.bfg_xOffset;
                    size_t yPos = y + yy + glyph.bfg_yOffset - 1;
                    LCD_ENCODE_COLOR(xPos + yPos * xMax, color);
                }
            }

            x += glyph.bfg_xAdvance;
        }
    }

    // Fill to the end of the line
    for(size_t yy = y - lineHeight; yy < y; ++yy) {
        for(size_t xx = x; xx < xMax; ++xx) {
            LCD_ENCODE_COLOR(xx + yy * xMax, op->lo_bg);
        }
    }

    tft_set_window(op->lo_x, op->lo_y, op->lo_x + xMax - 1, op->lo_y + y - 1);
    tft_dma_memmode(1);
    tft_lcd_dma(tft_lcdBuffer, xMax * y);
}

void tft_render_op(struct LcdOperation* op) {
    switch(op->lo_op) {
        case RECT_FILL: {
            size_t modifiedPixels = op->lo_rect.width * op->lo_rect.height;
            if(modifiedPixels > LCD_MAX_DMA_TRANSFER) {
                size_t maxLines = (LCD_MAX_DMA_TRANSFER) / op->lo_rect.width;

                modifiedPixels = maxLines * op->lo_rect.width;
                tft_set_window(op->lo_x, op->lo_y, op->lo_x + op->lo_rect.width - 1, op->lo_y + maxLines - 1);

                struct LcdOperation* fillContinue = tft_new_operation(RECT_FILL);
                fillContinue->lo_x = op->lo_x;
                fillContinue->lo_y = op->lo_y + maxLines;
                fillContinue->lo_rect.width = op->lo_rect.width;
                fillContinue->lo_rect.height = op->lo_rect.height - maxLines;
                fillContinue->lo_fg = op->lo_fg;

                tft_insert_after(op, fillContinue);
            } else {
                tft_set_window(op->lo_x, op->lo_y, op->lo_x + op->lo_rect.width - 1, op->lo_y + op->lo_rect.height - 1);
            }

            tft_dma_memmode(0);
            tft_lcd_dma(&op->lo_fg, modifiedPixels);
        } break;
        case TEXT: {
            tft_render_text(op);
        } break;
    }
}

/********** End of rendering functions **********/

void tft_lcd_dma_complete() {
    // Take the current operation out of queue
    struct LcdOperation* oldOp = tft_lcdOperations;
    tft_lcdOperations = tft_lcdOperations->lo_next;
    // Don't free static operations
    if(!oldOp->lo_static)
        free(oldOp);

    // Wait for SPI to finish doing it's thing
    SPI_WAIT_NBSY();

    if(tft_lcdOperations) {
        // Render next operation
        tft_render_op(tft_lcdOperations);
    } else {
        // Finish the transfer
        LCD_DESELECT();
    }
}

void tft_start_render() {
    tft_render_op(tft_lcdOperations);
}

void tft_driver_init() {
    /************ Configure on-chip peripherals **************/

    // Enable clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    // Initialize the GPIO
    GPIO_InitTypeDef GpioOut;
    GpioOut.Pin = LCD_SS;
    GpioOut.Mode = GPIO_MODE_OUTPUT_PP;
    GpioOut.Pull = GPIO_NOPULL;
    GpioOut.Speed = GPIO_SPEED_HIGH;
    HAL_GPIO_Init(GPIOA, &GpioOut);

    GpioOut.Pin = LCD_DC | LCD_RST;
    HAL_GPIO_Init(GPIOB, &GpioOut);

    GPIO_InitTypeDef GpioAf;
    GpioAf.Pin = LCD_MOSI | LCD_MISO;
    GpioAf.Mode = GPIO_MODE_AF_PP;
    GpioAf.Pull = GPIO_NOPULL;
    GpioAf.Speed = GPIO_SPEED_HIGH;
    GpioAf.Alternate = 5;
    HAL_GPIO_Init(GPIOA, &GpioAf);

    GpioAf.Pin = LCD_SCK;
    GpioAf.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GpioAf);

    LCD_DESELECT();
    LCD_DEASSERT_RST();
    LCD_DELAY(5);

    // Initialize the SPI interface
    __HAL_RCC_SPI1_FORCE_RESET();
    __HAL_RCC_SPI1_RELEASE_RESET();

    // Initialize the SPI interface
    tft_lcdSPI.Instance = SPI1;
    tft_lcdSPI.Init.Mode = SPI_MODE_MASTER;
    tft_lcdSPI.Init.CLKPolarity = SPI_POLARITY_LOW;
    tft_lcdSPI.Init.CLKPhase = SPI_PHASE_1EDGE;
    tft_lcdSPI.Init.Direction = SPI_DIRECTION_2LINES;
    tft_lcdSPI.Init.DataSize = SPI_DATASIZE_8BIT;
    tft_lcdSPI.Init.NSS = SPI_NSS_SOFT;
    tft_lcdSPI.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    tft_lcdSPI.Init.FirstBit = SPI_FIRSTBIT_MSB;
    tft_lcdSPI.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLED;
    HAL_SPI_Init(&tft_lcdSPI);

    // Initialize DMA Stream
    tft_lcdDMA.Instance = DMA2_Stream2;
    tft_lcdDMA.Init.Mode = DMA_NORMAL;
    tft_lcdDMA.Init.Channel = DMA_CHANNEL_2;
    tft_lcdDMA.Init.Direction = DMA_MEMORY_TO_PERIPH;
    tft_lcdDMA.Init.PeriphInc = DMA_PINC_DISABLE;
    tft_lcdDMA.Init.MemInc = DMA_MINC_ENABLE;
    tft_lcdDMA.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    tft_lcdDMA.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    tft_lcdDMA.Init.PeriphBurst = DMA_PBURST_SINGLE;
    tft_lcdDMA.Init.MemBurst = DMA_MBURST_SINGLE;
    tft_lcdDMA.Init.Priority = DMA_PRIORITY_HIGH;
    tft_lcdDMA.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&tft_lcdDMA);
    __HAL_LINKDMA(&tft_lcdSPI, hdmatx, tft_lcdDMA);

    // Setup DMA interrupts
    HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);

    /****************** Configure TFT LCD *********************/

    // First, reset the LCD panel
    LCD_ASSERT_RST();
    LCD_DELAY(20);
    LCD_DEASSERT_RST();
    LCD_DELAY(150);

    // LCD Software Reset
    tft_lcd_cmd(0x01);
    LCD_DELAY(120);

    // LCD Sleep out command
    tft_lcd_cmd(0x11);
    LCD_DELAY(120);

    for(size_t i = 0; i < sizeof(TFT_Init_Sequence); ++i) {
        if(TFT_Init_Sequence[i++] == 0xFF && i < sizeof(TFT_Init_Sequence)) {
            uint8_t cmd = TFT_Init_Sequence[i++];
            const void* dataStart = TFT_Init_Sequence + i;
            const void* dataEnd = memchr(dataStart, 0xFF, sizeof(TFT_Init_Sequence) - i);
            if(dataEnd == dataStart) {
                tft_lcd_cmd(cmd);
                --i;
            } else {
                tft_lcd_cmd_data(cmd, dataStart, (uint8_t*)dataEnd - (uint8_t*)dataStart);
                i += (uint8_t*)dataEnd - (uint8_t*)dataStart - 1;
            }
        }
    }

    // Display on
    tft_lcd_cmd(0x29);
    LCD_DELAY(150);

    // Switch SPI into 16 bit mode for faster data transfer
    tft_lcdSPI.Init.DataSize = SPI_DATASIZE_16BIT;
    HAL_SPI_Init(&tft_lcdSPI);
}

void DMA2_Stream2_IRQHandler() {
    HAL_DMA_IRQHandler(&tft_lcdDMA);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi) {
    tft_lcd_dma_complete();
}
