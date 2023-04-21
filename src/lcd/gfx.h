#ifndef MODULES_GFX_H
#define MODULES_GFX_H

#include "tft_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GuiElementType_t {
    GFX_BOX,
    GFX_BUTTON,
    GFX_TEXT,
    GFX_BORDER,
    GFX_IMAGE_BUTTON
} GuiElementType;

typedef void callback_t(const void* element);

#define POSITION_NUMBER_PART 0x1FF
#define POSITION_SIGN_BIT 0x200

#define RELATIVE_TO_MASK 0xFC00
#define RELATIVE_TO_WIDTH 0x0400
#define RELATIVE_TO_HEIGHT 0x0800

#define PARENT_WIDTH(offset) ((offset & 0x3FF) | RELATIVE_TO_WIDTH)
#define PARENT_HEIGHT(offset) ((offset & 0x3FF) | RELATIVE_TO_HEIGHT)

#define ALIGN_LEFT    0
#define ALIGN_CENTER  1
#define ALIGN_RIGHT   2

struct GuiElement {
    GuiElementType ge_type;

    int16_t ge_x;
    int16_t ge_y;

    uint16_t ge_width;
    uint16_t ge_height;

    LcdColor ge_color;

    union {
        /* Button */
        struct {
            LcdColor ge_textColor;
            const char* ge_text;
            const struct BitmapFont* ge_font;
            callback_t* ge_clickCallback;
            uint8_t ge_textAlign;
        } ge_button;
        /* Box */
        struct {
            size_t ge_childrenCount;
            const struct GuiElement* ge_children;
        } ge_box;
        /* Text */
        struct {
            const char* ge_text;
            const struct BitmapFont* ge_font;
            uint8_t ge_textAlign;
        } ge_text;
        /* Border */
        struct {
            uint8_t ge_borderThickness;
        } ge_border;
        /* Image button */
        struct {
            LcdColor ge_fgColor;
            const void* ge_bitmap;
            callback_t* ge_clickCallback;
            uint8_t ge_rle;
            uint8_t ge_imageScale;
        } ge_img_button;
    };

    uint8_t ge_dirty;
};

typedef struct GfxRenderChain_t {
    struct LcdOperation* grc_operations;
    size_t grc_length;
    void* grc_eventList;
} GfxRenderChain;

extern GfxRenderChain gfx_create_render_chain(const struct GuiElement* elements, size_t elementCount);
extern GfxRenderChain gfx_create_update_chain(const struct GuiElement* elements, size_t elementCount);

extern void gfx_activate_event_list(void* eventList);

extern void gfx_delete_render_chain(GfxRenderChain chain);

#define GUI_BUTTON(x, y, width, height, color, textcolor, text, align, font, clickcb) \
    { GFX_BUTTON, x, y, width, height, \
      .ge_color = color, \
      .ge_button = { \
        .ge_textColor = textcolor, \
        .ge_text = text, \
        .ge_font = &font, \
        .ge_clickCallback = clickcb, \
        .ge_textAlign = align } }
#define GUI_TEXT(x, y, color, text, font) \
    { GFX_TEXT, x, y, \
      .ge_color = color, \
      .ge_text = { \
        .ge_text = text, \
        .ge_font = &font } }
#define GUI_TEXT_CENTERED(x, y, width, color, text, font) \
    { GFX_TEXT, x, y, width, \
      .ge_color = color, \
      .ge_text = { \
        .ge_text = text, \
        .ge_font = &font, \
        .ge_textAlign = 1 } }
#define GUI_TEXT_RIGHT(x, y, color, text, font) \
    { GFX_TEXT, x, y, \
      .ge_color = color, \
      .ge_text = { \
        .ge_text = text, \
        .ge_font = &font, \
        .ge_textAlign = 2 } }
#define GUI_BOX_STATIC(x, y, width, height, color, children) \
    { GFX_BOX, x, y, width, height, \
      .ge_color = color, \
      .ge_box = { \
        .ge_childrenCount = sizeof(children) / sizeof(struct GuiElement), \
        .ge_children = children } }
#define GUI_BORDER(x, y, width, height, color, thickness) \
    { GFX_BORDER, x, y, width, height, \
      .ge_color = color, \
      .ge_border = { \
        .ge_borderThickness = thickness } }
#define GUI_IMAGE_BUTTON(x, y, width, height, scale, bgColor, fgColor, bitmap, rle, clickcb) \
    { GFX_IMAGE_BUTTON, x, y, width, height, \
      .ge_color = bgColor, \
      .ge_img_button = { \
        .ge_fgColor = fgColor, \
        .ge_bitmap = bitmap, \
        .ge_clickCallback = clickcb, \
        .ge_rle = rle, \
        .ge_imageScale = scale } }

#ifdef __cplusplus
}
#endif

#endif
