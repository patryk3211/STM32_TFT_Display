#ifndef MODULES_TFT_DRIVER_H
#define MODULES_TFT_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "font.h"

#define LCD_BUFFER_SIZE 16 * 1024
#define LCD_MAX_DMA_TRANSFER 32 * 1024 - 2

typedef union LcdColor_t {
    uint16_t word;
    struct {
        uint16_t r:5;
        uint16_t g:5;
        uint16_t zero:1;
        uint16_t b:5;
    };
} LcdColor;

#define TFT_WIDTH 320
#define TFT_HEIGHT 480
#define TFT_MEMCTL 0x40

#define TFT_BLACK   (LcdColor) { 0b0000000000000000 }
#define TFT_WHITE   (LcdColor) { 0b1111111111011111 }
#define TFT_RED     (LcdColor) { 0b0000000000011111 }
#define TFT_GREEN   (LcdColor) { 0b0000011111000000 }
#define TFT_BLUE    (LcdColor) { 0b1111100000000000 }
#define TFT_YELLOW  (LcdColor) { 0b0000011111011111 }
#define TFT_MAGENTA (LcdColor) { 0b1111100000011111 }
#define TFT_CYAN    (LcdColor) { 0b1111111111000000 }

#define TFT_COLOR(r, g, b) (LcdColor) { ((b & 0x1F) << 11) | ((g & 0x1F) << 6) | (r & 0x1F) }

#define FONT_Y_OFFSET 6

static inline LcdColor tft_colmul(LcdColor color, float scalar) {
    LcdColor result;
    result.r = (float)color.r * scalar;
    result.g = (float)color.g * scalar;
    result.b = (float)color.b * scalar;
    return result;
}

typedef enum LcdOperationEnum_t {
    RECT_FILL,
    TEXT,
    BITMAP,
    BITMAP_CONTINUE,
    RLE_BITMAP,
    RLE_BITMAP_CONTINUE
} LcdOperationEnum;

struct LcdOperation {
    LcdOperationEnum lo_op;
    LcdColor lo_fg;
    LcdColor lo_bg;

    char lo_static;
    struct LcdOperation* lo_next;

    uint16_t lo_x;
    uint16_t lo_y;

    union {
        struct {
            uint16_t width;
            uint16_t height;
        } lo_rect;
        struct {
            const char* value;
            const struct BitmapFont* font;
        } lo_text;
        struct {
            uint16_t width;
            uint16_t height;
            const void* bitmap;
            uint8_t scale;
        } lo_bitmap;
        struct {
            uint16_t width;
            uint16_t height;
            const void* bitmap;
            uint8_t scale;
            size_t bitmapOffset;
            uint8_t bits;
            uint8_t mask;
            size_t lengthLeft;
        } lo_bitmap_cont;
    };
};

extern void tft_driver_init(void);

/**
 * @brief Create new LCD operation
 * Allocates and returns a pointer to an LCD operation structure
 *
 * @param operation LCD operation type
 * @return struct LcdOperation* Pointer to operation struct
 */
extern struct LcdOperation* tft_new_operation(LcdOperationEnum operation);

/**
 * @brief Submit LCD operation
 * Submits an LCD operation to the render queue.
 *
 * @param op Operation to submit
 */
extern void tft_submit(struct LcdOperation* op);

/**
 * @brief Submit LCD operations
 * Submits multiple LCD operations to the render queue,
 * the operations are not freed after the render is finished.
 *
 * @param ops Array of operations
 * @param count Length of the array
 */
extern void tft_submit_multiple(struct LcdOperation* ops, size_t count);

/**
 * @brief Start render
 * Renders all queued operations onto the display, all
 * rendered operations are taken out of the queue and
 * freed (if lo_static is false)
 */
extern void tft_start_render();

/**
 * @brief Recalibrate TouchPanel
 * This function will start the touch panel calibration procedure,
 * you will have to click 4 spots which will appear on the display.
 */
extern void tft_recalibrate_tp();

/**
 * @brief TFT event loop
 * This method has to be periodically called to service
 * any touch events that happen.
 */
extern void tft_main_loop();

/**
 * @brief TouchPanel interrupt handler
 * You should register this method as the interrupt
 * handler for the TP Irq pin on the falling edge.
 */
extern void tft_tp_irq();

#ifdef __cplusplus
}
#endif

#endif
