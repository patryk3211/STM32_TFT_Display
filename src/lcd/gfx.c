#include "gfx.h"

#include <string.h>

#define ALLOC(type) malloc(sizeof(struct type))
#define DEF_HANDLE_TYPE(typename) void gfx_handle_##typename(const struct GuiElement* element, Context* context)
#define HANDLE_TYPE(typename) case typename: gfx_handle_##typename(element, context); break
#define BASE_INFO(listentry, optype) \
    (listentry)->operation.lo_op = (optype); \
    (listentry)->operation.lo_fg = element->ge_color; \
    (listentry)->operation.lo_bg = context->c_prevColor; \
    (listentry)->operation.lo_x = element->ge_x + context->c_x; \
    (listentry)->operation.lo_y = element->ge_y + context->c_y

size_t gfx_listLength = 0;
struct OpListEntry* gfx_list = 0;
struct OpListEntry* gfx_listLast = 0;
struct EventListEntry* gfx_buildingEventList = 0;
struct EventListEntry* gfx_buildingEventListLast = 0;

struct EventListEntry* gfx_eventList = 0;

struct OpListEntry {
    struct LcdOperation operation;
    struct OpListEntry* next;
};

struct EventListEntry {
    uint16_t x, y, width, height;
    callback_t* callback;

    const struct GuiElement* element;

    struct EventListEntry* next;
};

typedef struct Context_t {
    uint16_t c_x;
    uint16_t c_y;

    LcdColor c_prevColor;
} Context;

void gfx_insert_op(struct OpListEntry* entry) {
    if(!gfx_list) {
        gfx_list = entry;
        gfx_listLast = entry;
    } else {
        gfx_listLast->next = entry;
        gfx_listLast = entry;
    }
    entry->next = 0;
    ++gfx_listLength;
}

void gfx_insert_ev(struct EventListEntry* entry) {
    if(!gfx_buildingEventList) {
        gfx_buildingEventList = entry;
        gfx_buildingEventListLast = entry;
    } else {
        gfx_buildingEventListLast->next = entry;
        gfx_buildingEventListLast = entry;
    }
    entry->next = 0;
}

void gfx_handle_element(const struct GuiElement* element, Context* context);

DEF_HANDLE_TYPE(GFX_BOX) {
    struct OpListEntry* e = ALLOC(OpListEntry);

    BASE_INFO(e, RECT_FILL);
    e->operation.lo_rect.width = element->ge_width;
    e->operation.lo_rect.height = element->ge_height;

    gfx_insert_op(e);

    Context ctx_new;
    ctx_new.c_x = e->operation.lo_x;
    ctx_new.c_y = e->operation.lo_y;
    ctx_new.c_prevColor = element->ge_color;

    for(size_t i = 0; i < element->ge_childrenCount; ++i)
        gfx_handle_element(element->ge_children + i, &ctx_new);
}

DEF_HANDLE_TYPE(GFX_TEXT) {
    struct OpListEntry* e = ALLOC(OpListEntry);
    BASE_INFO(e, TEXT);

    e->operation.lo_text.value = element->ge_text;
    e->operation.lo_text.font = element->ge_font;

    gfx_insert_op(e);
}

DEF_HANDLE_TYPE(GFX_BUTTON) {
    struct OpListEntry* bg = ALLOC(OpListEntry);

    BASE_INFO(bg, RECT_FILL);
    bg->operation.lo_rect.width = element->ge_width;
    bg->operation.lo_rect.height = element->ge_height;

    gfx_insert_op(bg);

    struct OpListEntry* text = ALLOC(OpListEntry);

    BASE_INFO(text, TEXT);
    text->operation.lo_bg = element->ge_color;
    text->operation.lo_fg = element->ge_color2;

    text->operation.lo_text.value = element->ge_text;
    text->operation.lo_text.font = element->ge_font;

    // Calculate text length
    size_t len = 0;
    for(const char* c = element->ge_text; *c; ++c) {
        if(*c < element->ge_font->bf_firstChar || *c > element->ge_font->bf_lastChar)
            continue;

        struct BitmapFontGlyph glyph = element->ge_font->bf_glyphs[*c - element->ge_font->bf_firstChar];
        len += glyph.bfg_xAdvance;
    }

    if(len < element->ge_width) {
        // We can center the text inside of the container horizontally
        size_t emptyLen = element->ge_width - len;
        text->operation.lo_x += emptyLen / 2;
    }

    if(element->ge_font->bf_yAdvance < element->ge_height) {
        // We can center text inside of the container vertically
        size_t emptyHeight = element->ge_height - element->ge_font->bf_yAdvance;
        text->operation.lo_y += emptyHeight / 2;
    }

    // Create the event list entry
    if(element->ge_click_cb != 0) {
        struct EventListEntry* evEn = ALLOC(EventListEntry);

        evEn->x = bg->operation.lo_x;
        evEn->y = bg->operation.lo_y;
        evEn->width = element->ge_width;
        evEn->height = element->ge_height;
        evEn->callback = element->ge_click_cb;
        evEn->element = element;

        gfx_insert_ev(evEn);
    }

    gfx_insert_op(text);
}

void gfx_handle_element(const struct GuiElement* element, Context* context) {
    switch(element->ge_type) {
        HANDLE_TYPE(GFX_BOX);
        HANDLE_TYPE(GFX_TEXT);
        HANDLE_TYPE(GFX_BUTTON);
    }
}

GfxRenderChain gfx_create_render_chain(const struct GuiElement* elements, size_t element_count) {
    // Create a base context
    Context ctx;
    ctx.c_x = 0;
    ctx.c_y = 0;
    ctx.c_prevColor = TFT_BLACK;

    for(size_t i = 0; i < element_count; ++i)
        gfx_handle_element(elements + i, &ctx);

    // Collect the list into an array
    struct LcdOperation* ops = malloc(sizeof(struct LcdOperation) * gfx_listLength);
    size_t index = 0;

    struct OpListEntry* entry = gfx_list;
    while(entry) {
        struct OpListEntry* next = entry->next;
        memcpy(ops + index, &entry->operation, sizeof(struct LcdOperation));
        free(entry);

        entry = next;
        ++index;
    }

    GfxRenderChain result;
    result.grc_length = gfx_listLength;
    result.grc_operations = ops;
    result.grc_eventList = gfx_buildingEventList;

    gfx_list = 0;
    gfx_listLast = 0;
    gfx_listLength = 0;
    gfx_buildingEventList = 0;
    gfx_buildingEventListLast = 0;

    return result;
}

void gfx_activate_event_list(void* eventList) {
    gfx_eventList = eventList;
}

void tft_touch_cb(uint16_t x, uint16_t y) {
    for(struct EventListEntry* en = gfx_eventList; en; en = en->next) {
        if(x >= en->x && y >= en->y && x < en->x + en->width && y < en->y + en->height) {
            en->callback(en->element);
            break;
        }
    }
}
