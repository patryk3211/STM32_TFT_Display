#ifndef MODULES_TFT_DRIVER_H
#define MODULES_TFT_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "font.h"

#define LCD_BUFFER_SIZE 16 * 1024
#define LCD_MAX_DMA_TRANSFER 32 * 1024 - 2

typedef enum LcdOperationEnum_t {
    RECT_FILL,
    TEXT
} LcdOperationEnum;

typedef union LcdColor_t {
    uint16_t word;
    struct {
        uint16_t r:5;
        uint16_t g:5;
        uint16_t zero:1;
        uint16_t b:5;
    };
} LcdColor;

#define TFT_BLACK   (LcdColor) { 0b0000000000000000 }
#define TFT_WHITE   (LcdColor) { 0b1111111111011111 }
#define TFT_RED     (LcdColor) { 0b0000000000011111 }
#define TFT_GREEN   (LcdColor) { 0b0000011111000000 }
#define TFT_BLUE    (LcdColor) { 0b1111100000000000 }
#define TFT_YELLOW  (LcdColor) { 0b0000011111011111 }
#define TFT_MAGENTA (LcdColor) { 0b1111100000011111 }
#define TFT_CYAN    (LcdColor) { 0b1111111111000000 }

static inline LcdColor tft_colmul(LcdColor color, float scalar) {
    LcdColor result;
    result.r = (float)color.r * scalar;
    result.g = (float)color.g * scalar;
    result.b = (float)color.b * scalar;
    return result;
}

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
extern void tft_submit_multiple(struct LcdOperation* ops, unsigned int count);

/**
 * @brief Start render
 * Renders all queued operations onto the display, all
 * rendered operations are taken out of the queue and
 * freed (if lo_static is false)
 */
extern void tft_start_render();

#ifdef __cplusplus
}
#endif

#endif
