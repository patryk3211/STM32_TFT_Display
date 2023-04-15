#ifndef MODULES_GFX_H
#define MODULES_GFX_H

#include "tft_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GuiElementType_t {
    GFX_BOX,
    GFX_BUTTON,
    GFX_TEXT
} GuiElementType;

typedef void callback_t(const void* element);

struct GuiElement {
    GuiElementType ge_type;

    uint16_t ge_x;
    uint16_t ge_y;

    uint16_t ge_width;
    uint16_t ge_height;

    LcdColor ge_color;
    LcdColor ge_color2;

    const struct BitmapFont* ge_font;
    const char* ge_text;

    callback_t* ge_click_cb;

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

#ifdef __cplusplus
}
#endif

#endif
