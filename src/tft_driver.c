#include <Arduino.h>

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

#define SPI_WAIT_NBSY() while(__HAL_SPI_GET_FLAG(&bbm_lcdSPI, SPI_FLAG_BSY))

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
SPI_HandleTypeDef bbm_lcdSPI;
DMA_HandleTypeDef bbm_lcdDMA;

uint8_t bbm_lcdBuffer[LCD_BUFFER_SIZE * LCD_BYTES_PER_PIXEL]; // We'll allocate a 16K buffer for drawing things

struct LcdOperation* bbm_lcdOperations = 0;
struct LcdOperation* bbm_lcdLastOp = 0;

/********** Functions **********/

/**
 * @brief LCD command
 * Sends a simple LCD command (1 byte)
 *
 * @param cmd Command
 */
void bbm_lcd_cmd(uint8_t cmd) {
    LCD_SELECT();

    LCD_MODE_CMD();
    uint8_t buffer[2];
    buffer[0] = 0;
    buffer[1] = cmd;
    HAL_SPI_Transmit(&bbm_lcdSPI, buffer, 2, 10);
    SPI_WAIT_NBSY();

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
void bbm_lcd_cmd_data(uint8_t cmd, const void* data, size_t length) {
    LCD_SELECT();

    LCD_MODE_CMD();
    uint8_t buffer[2];
    buffer[0] = 0;
    buffer[1] = cmd;
    HAL_SPI_Transmit(&bbm_lcdSPI, buffer, 2, 10);
    SPI_WAIT_NBSY();

    LCD_MODE_DATA();
    HAL_SPI_Transmit(&bbm_lcdSPI, (uint8_t*)data, length, 10);
    SPI_WAIT_NBSY();

    LCD_DESELECT();
}

/**
 * @brief LCD data
 * Sends data to LCD using DMA
 *
 * @param data Data
 * @param length Length of data
 */
void bbm_lcd_data(const void* data, size_t length) {
    LCD_SELECT();
    LCD_MODE_DATA();

    // Start the DMA transfer
    HAL_SPI_Transmit_DMA(&bbm_lcdSPI, (uint8_t*)data, length);
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
void bbm_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[8];
    memset(data, 0, sizeof(data));

    data[1] = x0 >> 8;
    data[3] = x0 & 0xFF;
    data[5] = x1 >> 8;
    data[7] = x1 & 0xFF;
    bbm_lcd_cmd_data(0x2A, data, 8);

    data[1] = y0 >> 8;
    data[3] = y0 & 0xFF;
    data[5] = y1 >> 8;
    data[7] = y1 & 0xFF;
    bbm_lcd_cmd_data(0x2B, data, 8);

    bbm_lcd_cmd(0x2C);
}

struct LcdOperation* bbm_new_operation(LcdOperationEnum operation) {
    struct LcdOperation* lop = (struct LcdOperation*)malloc(sizeof(struct LcdOperation));

    lop->lo_op = operation;
    lop->lo_next = 0;
    lop->lo_static = 0;

    return lop;
}

void bbm_submit(struct LcdOperation* op) {
    if(!bbm_lcdOperations) {
        bbm_lcdOperations = op;
        bbm_lcdLastOp = op;
    } else {
        bbm_lcdLastOp->lo_next = op;
        bbm_lcdLastOp = op;
    }
}

void bbm_render_op(struct LcdOperation* op) {
    switch(op->lo_op) {
        case RECT_FILL: {
            size_t modifiedPixels = op->lo_width * op->lo_height;
            if(modifiedPixels > LCD_BUFFER_SIZE) {
                size_t maxLines = LCD_BUFFER_SIZE / op->lo_width;

                modifiedPixels = maxLines * op->lo_width;
                bbm_set_window(op->lo_x, op->lo_y, op->lo_x + op->lo_width - 1, op->lo_y + maxLines - 1);

                struct LcdOperation* fillContinue = bbm_new_operation(RECT_FILL);
                fillContinue->lo_x = op->lo_x;
                fillContinue->lo_y = op->lo_y + maxLines;
                fillContinue->lo_width = op->lo_width;
                fillContinue->lo_height = op->lo_height - maxLines;
                fillContinue->lo_color = op->lo_color;

                if(op == bbm_lcdLastOp)
                    bbm_lcdLastOp = fillContinue;
                fillContinue->lo_next = op->lo_next;
                op->lo_next = fillContinue;
            } else {
                bbm_set_window(op->lo_x, op->lo_y, op->lo_x + op->lo_width - 1, op->lo_y + op->lo_height - 1);
            }

            for(size_t i = 0; i < modifiedPixels; ++i) {
                bbm_lcdBuffer[i * LCD_BYTES_PER_PIXEL] =
                     (op->lo_color.r << 3) |
                    ((op->lo_color.g >> 2) & 0x7);
                bbm_lcdBuffer[i * LCD_BYTES_PER_PIXEL + 1] =
                    ((op->lo_color.g & 0x3) << 6) |
                      op->lo_color.b;
            }

            bbm_lcd_data(bbm_lcdBuffer, modifiedPixels * LCD_BYTES_PER_PIXEL);
        } break;
    }

    // Take the operation out of queue
    bbm_lcdOperations = op->lo_next;
    // Don't free static operations
    if(!op->lo_static)
        free(op);
}

void bbm_lcd_dma_complete() {
    // Wait for SPI to finish doing it's thing
    SPI_WAIT_NBSY();

    if(bbm_lcdOperations) {
        // Render next operation
        bbm_render_op(bbm_lcdOperations);
    } else {
        // Finish the transfer
        LCD_DESELECT();
    }
}

void bbm_start_render() {
    bbm_render_op(bbm_lcdOperations);
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
    bbm_lcdSPI.Instance = SPI1;
    bbm_lcdSPI.Init.Mode = SPI_MODE_MASTER;
    bbm_lcdSPI.Init.CLKPolarity = SPI_POLARITY_LOW;
    bbm_lcdSPI.Init.CLKPhase = SPI_PHASE_1EDGE;
    bbm_lcdSPI.Init.Direction = SPI_DIRECTION_2LINES;
    bbm_lcdSPI.Init.DataSize = SPI_DATASIZE_8BIT;
    bbm_lcdSPI.Init.NSS = SPI_NSS_SOFT;
    bbm_lcdSPI.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    bbm_lcdSPI.Init.FirstBit = SPI_FIRSTBIT_MSB;
    bbm_lcdSPI.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLED;
    HAL_SPI_Init(&bbm_lcdSPI);

    // Initialize DMA Stream
    bbm_lcdDMA.Instance = DMA2_Stream2;
    bbm_lcdDMA.Init.Mode = DMA_NORMAL;
    bbm_lcdDMA.Init.Channel = DMA_CHANNEL_2;
    bbm_lcdDMA.Init.Direction = DMA_MEMORY_TO_PERIPH;
    bbm_lcdDMA.Init.PeriphInc = DMA_PINC_DISABLE;
    bbm_lcdDMA.Init.MemInc = DMA_MINC_ENABLE;
    bbm_lcdDMA.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    bbm_lcdDMA.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    bbm_lcdDMA.Init.PeriphBurst = DMA_PBURST_SINGLE;
    bbm_lcdDMA.Init.MemBurst = DMA_MBURST_SINGLE;
    bbm_lcdDMA.Init.Priority = DMA_PRIORITY_HIGH;
    bbm_lcdDMA.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&bbm_lcdDMA);
    __HAL_LINKDMA(&bbm_lcdSPI, hdmatx, bbm_lcdDMA);

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
    bbm_lcd_cmd(0x01);
    LCD_DELAY(120);

    // LCD Sleep out command
    bbm_lcd_cmd(0x11);
    LCD_DELAY(120);

    for(size_t i = 0; i < sizeof(TFT_Init_Sequence); ++i) {
        if(TFT_Init_Sequence[i++] == 0xFF && i < sizeof(TFT_Init_Sequence)) {
            uint8_t cmd = TFT_Init_Sequence[i++];
            const void* dataStart = TFT_Init_Sequence + i;
            const void* dataEnd = memchr(dataStart, 0xFF, sizeof(TFT_Init_Sequence) - i);
            if(dataEnd == dataStart) {
                bbm_lcd_cmd(cmd);
                --i;
            } else {
                bbm_lcd_cmd_data(cmd, dataStart, (char*)dataEnd - (char*)dataStart);
                i += (char*)dataEnd - (char*)dataStart - 1;
            }
        }
    }

    // Display on
    bbm_lcd_cmd(0x29);
    LCD_DELAY(150);
}

void DMA2_Stream2_IRQHandler() {
    HAL_DMA_IRQHandler(&bbm_lcdDMA);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi) {
    bbm_lcd_dma_complete();
}
