#ifndef MODULES_TFT_DRIVER_H
#define MODULES_TFT_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "font.h"

#define LCD_BUFFER_SIZE 16 * 1024
#define LCD_BYTES_PER_PIXEL 2

typedef enum LcdOperationEnum_t {
    RECT_FILL,
    TEXT
} LcdOperationEnum;

struct LcdOperation {
    LcdOperationEnum lo_op;
    union {
        uint16_t word;
        struct {
            uint16_t r:5;
            uint16_t g:5;
            uint16_t b:5;
        };
    } lo_fg;

    union {
        uint16_t word;
        struct {
            uint16_t r:5;
            uint16_t g:5;
            uint16_t b:5;
        };
    } lo_bg;

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
 * @brief Submit LCD operation(s)
 * Submits an LCD operation (multiple of lo_next is not zero)
 * to the render queue.
 *
 * @param op Operation to submit
 */
extern void tft_submit(struct LcdOperation* op);

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
