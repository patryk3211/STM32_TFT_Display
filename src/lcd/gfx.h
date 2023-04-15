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
    GFX_BORDER
} GuiElementType;

typedef void callback_t(const void* element);

#define POSITION_NUMBER_PART 0x1FF
#define POSITION_SIGN_BIT 0x200

#define RELATIVE_TO_MASK 0xFC00
#define RELATIVE_TO_WIDTH 0x0400
#define RELATIVE_TO_HEIGHT 0x0800

#define PARENT_WIDTH(offset) ((offset & 0x3FF) | RELATIVE_TO_WIDTH)
#define PARENT_HEIGHT(offset) ((offset & 0x3FF) | RELATIVE_TO_HEIGHT)

struct GuiElement {
    GuiElementType ge_type;

    int16_t ge_x;
    int16_t ge_y;

    uint16_t ge_width;
    uint16_t ge_height;

    LcdColor ge_color;
    LcdColor ge_color2;

    const struct BitmapFont* ge_font;
    const char* ge_text;

    uint8_t ge_textAlign;

    callback_t* ge_click_cb;

    uint8_t ge_thickness;

    size_t ge_childrenCount;
    const struct GuiElement* ge_children;
};

typedef struct GfxRenderChain_t {
    struct LcdOperation* grc_operations;
    size_t grc_length;
    void* grc_eventList;
} GfxRenderChain;

extern GfxRenderChain gfx_create_render_chain(const struct GuiElement* elements, size_t elementCount);

extern void gfx_activate_event_list(void* eventList);

#define GUI_BUTTON(x, y, width, height, color, textcolor, text, font, clickcb) \
    { GFX_BUTTON, x, y, width, height, \
      .ge_color = color, \
      .ge_color2 = textcolor, \
      .ge_font = &font, \
      .ge_text = text, \
      .ge_click_cb = clickcb }
#define GUI_TEXT(x, y, color, text, font) \
    { GFX_TEXT, x, y, \
      .ge_color = color, \
      .ge_font = &font, \
      .ge_text = text }
#define GUI_TEXT_CENTERED(x, y, width, color, text, font) \
    { GFX_TEXT, x, y, width, \
      .ge_color = color, \
      .ge_font = &font, \
      .ge_text = text, \
      .ge_textAlign = 1 }
#define GUI_TEXT_RIGHT(x, y, color, text, font) \
    { GFX_TEXT, x, y, \
      .ge_color = color, \
      .ge_font = &font, \
      .ge_text = text, \
      .ge_textAlign = 2 }
#define GUI_BOX_STATIC(x, y, width, height, color, children) \
    { GFX_BOX, x, y, width, height, \
      .ge_color = color, \
      .ge_childrenCount = sizeof(children) / sizeof(struct GuiElement), \
      .ge_children = children }
#define GUI_BORDER(x, y, width, height, color, thickness) \
    { GFX_BORDER, x, y, width, height, \
      .ge_color = color, \
      .ge_thickness = thickness }

#ifdef __cplusplus
}
#endif

#endif
