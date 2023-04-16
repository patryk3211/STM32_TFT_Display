#include "gfx.h"

#include <string.h>

#define ALLOC(type) malloc(sizeof(struct type))
#define DEF_HANDLE_TYPE(typename) void gfx_handle_##typename(const struct GuiElement* element, Context* context)
#define HANDLE_TYPE(typename) case typename: gfx_handle_##typename(element, context); break
#define BASE_INFO(listentry, optype) \
    (listentry)->operation.lo_op = (optype); \
    (listentry)->operation.lo_fg = element->ge_color; \
    (listentry)->operation.lo_bg = context->c_prevColor; \
    (listentry)->operation.lo_x = gfx_decode_position(element->ge_x, context) + context->c_x; \
    (listentry)->operation.lo_y = gfx_decode_position(element->ge_y, context) + context->c_y

size_t gfx_listLength = 0;
struct OpListEntry* gfx_list = 0;
struct OpListEntry* gfx_listLast = 0;
struct EventListEntry* gfx_buildingEventList = 0;
struct EventListEntry* gfx_buildingEventListLast = 0;

struct EventListEntry* gfx_eventList = 0;

uint8_t gfx_onlyDirty = 0;

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

    uint16_t c_prevWidth;
    uint16_t c_prevHeight;

    LcdColor c_prevColor;

    uint8_t c_forceRender;
} Context;

int16_t gfx_decode_position(uint16_t encoded, Context* ctx) {
    int16_t numberPart = encoded & POSITION_NUMBER_PART;
    if(encoded & POSITION_SIGN_BIT) {
        // Sign bit set, negate the number
        numberPart |= ~POSITION_NUMBER_PART;
    }

    switch(encoded & RELATIVE_TO_MASK) {
        case RELATIVE_TO_WIDTH:
            return ctx->c_prevWidth + numberPart;
        case RELATIVE_TO_HEIGHT:
            return ctx->c_prevHeight + numberPart;
        case 0x0000:
        case RELATIVE_TO_MASK:
            // These two cases are either a normal number or a negative number,
            // they should be interpreted as a simple number.
            return numberPart;
        default:
            return 0;
    }
}

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
    Context ctx_new;

    ctx_new.c_x = gfx_decode_position(element->ge_x, context) + context->c_x;
    ctx_new.c_y = gfx_decode_position(element->ge_y, context) + context->c_y;
    ctx_new.c_prevWidth = gfx_decode_position(element->ge_width, context);
    ctx_new.c_prevHeight = gfx_decode_position(element->ge_height, context);
    ctx_new.c_prevColor = element->ge_color;
    ctx_new.c_forceRender = 0;

    if(!gfx_onlyDirty || element->ge_dirty || context->c_forceRender) {
        struct OpListEntry* e = ALLOC(OpListEntry);

        BASE_INFO(e, RECT_FILL);
        e->operation.lo_rect.width = ctx_new.c_prevWidth;
        e->operation.lo_rect.height = ctx_new.c_prevHeight;

        gfx_insert_op(e);

        ctx_new.c_forceRender = 1;
    }

    for(size_t i = 0; i < element->ge_box.ge_childrenCount; ++i)
        gfx_handle_element(element->ge_box.ge_children + i, &ctx_new);
}

size_t gfx_text_length(const char* text, const struct BitmapFont* font) {
    size_t len = 0;
    for(const char* c = text; *c; ++c) {
        if(*c < font->bf_firstChar || *c > font->bf_lastChar)
            continue;

        struct BitmapFontGlyph glyph = font->bf_glyphs[*c - font->bf_firstChar];
        len += glyph.bfg_xAdvance;
    }

    return len;
}

DEF_HANDLE_TYPE(GFX_TEXT) {
    struct OpListEntry* e = ALLOC(OpListEntry);
    BASE_INFO(e, TEXT);

    e->operation.lo_text.value = element->ge_text.ge_text;
    e->operation.lo_text.font = element->ge_text.ge_font;

    if(element->ge_text.ge_textAlign == 1) {
        // Calculate text length
        size_t len = gfx_text_length(element->ge_text.ge_text, element->ge_text.ge_font);
        size_t width = gfx_decode_position(element->ge_width, context);

        if(len < width) {
            // We can center the horizontally
            size_t emptyLen = width - len;
            e->operation.lo_x += emptyLen / 2;
        }
    } else if(element->ge_text.ge_textAlign == 2) {
        size_t len = gfx_text_length(element->ge_text.ge_text, element->ge_text.ge_font);

        // Offset text to the left by its length
        e->operation.lo_x -= len;
    }

    gfx_insert_op(e);
}

DEF_HANDLE_TYPE(GFX_BUTTON) {
    struct OpListEntry* bg = ALLOC(OpListEntry);

    BASE_INFO(bg, RECT_FILL);
    bg->operation.lo_rect.width = gfx_decode_position(element->ge_width, context);
    bg->operation.lo_rect.height = gfx_decode_position(element->ge_height, context);

    gfx_insert_op(bg);

    struct OpListEntry* text = ALLOC(OpListEntry);

    BASE_INFO(text, TEXT);
    text->operation.lo_bg = element->ge_color;
    text->operation.lo_fg = element->ge_button.ge_textColor;

    text->operation.lo_text.value = element->ge_button.ge_text;
    text->operation.lo_text.font = element->ge_button.ge_font;

    // Calculate text length
    size_t len = 0;
    for(const char* c = element->ge_button.ge_text; *c; ++c) {
        if(*c < element->ge_button.ge_font->bf_firstChar || *c > element->ge_button.ge_font->bf_lastChar)
            continue;

        struct BitmapFontGlyph glyph = element->ge_button.ge_font->bf_glyphs[*c - element->ge_button.ge_font->bf_firstChar];
        len += glyph.bfg_xAdvance;
    }

    if(len < bg->operation.lo_rect.width) {
        // We can center the text inside of the container horizontally
        size_t emptyLen = bg->operation.lo_rect.width - len;
        text->operation.lo_x += emptyLen / 2;
    }

    if(element->ge_button.ge_font->bf_yAdvance < bg->operation.lo_rect.height) {
        // We can center text inside of the container vertically
        size_t emptyHeight = bg->operation.lo_rect.height - element->ge_button.ge_font->bf_yAdvance;
        text->operation.lo_y += emptyHeight / 2;
    }

    // Create the event list entry
    if(element->ge_button.ge_clickCallback != 0 && !gfx_onlyDirty) {
        struct EventListEntry* evEn = ALLOC(EventListEntry);

        evEn->x = bg->operation.lo_x;
        evEn->y = bg->operation.lo_y;
        evEn->width = bg->operation.lo_rect.width;
        evEn->height = bg->operation.lo_rect.height;
        evEn->callback = element->ge_button.ge_clickCallback;
        evEn->element = element;

        gfx_insert_ev(evEn);
    }

    gfx_insert_op(text);
}

DEF_HANDLE_TYPE(GFX_BORDER) {
    size_t x = gfx_decode_position(element->ge_x, context) + context->c_x;
    size_t y = gfx_decode_position(element->ge_y, context) + context->c_y;
    size_t w = gfx_decode_position(element->ge_width, context);
    size_t h = gfx_decode_position(element->ge_height, context);

    struct OpListEntry* top = ALLOC(OpListEntry);
    top->operation.lo_op = RECT_FILL;
    top->operation.lo_fg = element->ge_color;
    top->operation.lo_x = x;
    top->operation.lo_y = y;
    top->operation.lo_rect.width = w;
    top->operation.lo_rect.height = element->ge_border.ge_borderThickness;
    gfx_insert_op(top);

    struct OpListEntry* bottom = ALLOC(OpListEntry);
    bottom->operation.lo_op = RECT_FILL;
    bottom->operation.lo_fg = element->ge_color;
    bottom->operation.lo_x = x;
    bottom->operation.lo_y = y + h - element->ge_border.ge_borderThickness;
    bottom->operation.lo_rect.width = w;
    bottom->operation.lo_rect.height = element->ge_border.ge_borderThickness;
    gfx_insert_op(bottom);

    struct OpListEntry* left = ALLOC(OpListEntry);
    left->operation.lo_op = RECT_FILL;
    left->operation.lo_fg = element->ge_color;
    left->operation.lo_x = x;
    left->operation.lo_y = y;
    left->operation.lo_rect.width = element->ge_border.ge_borderThickness;
    left->operation.lo_rect.height = h;
    gfx_insert_op(left);

    struct OpListEntry* right = ALLOC(OpListEntry);
    right->operation.lo_op = RECT_FILL;
    right->operation.lo_fg = element->ge_color;
    right->operation.lo_x = x + w - element->ge_border.ge_borderThickness;
    right->operation.lo_y = y;
    right->operation.lo_rect.width = element->ge_border.ge_borderThickness;
    right->operation.lo_rect.height = h;
    gfx_insert_op(right);
}

DEF_HANDLE_TYPE(GFX_IMAGE_BUTTON) {
    struct OpListEntry* bitmapOp = ALLOC(OpListEntry);

    LcdOperationEnum op = BITMAP;
    if(element->ge_img_button.ge_rle)
        op = RLE_BITMAP;

    size_t width = gfx_decode_position(element->ge_width, context);
    size_t height = gfx_decode_position(element->ge_height, context);

    BASE_INFO(bitmapOp, op);
    bitmapOp->operation.lo_bitmap.bitmap = element->ge_img_button.ge_bitmap;
    bitmapOp->operation.lo_bitmap.width = width / element->ge_img_button.ge_imageScale;
    bitmapOp->operation.lo_bitmap.height = height / element->ge_img_button.ge_imageScale;
    bitmapOp->operation.lo_bitmap.scale = element->ge_img_button.ge_imageScale;

    bitmapOp->operation.lo_fg = element->ge_img_button.ge_fgColor;
    bitmapOp->operation.lo_bg = element->ge_color;
    gfx_insert_op(bitmapOp);

    // Create the event list entry
    if(element->ge_img_button.ge_clickCallback != 0 && !gfx_onlyDirty) {
        struct EventListEntry* evEn = ALLOC(EventListEntry);

        evEn->x = bitmapOp->operation.lo_x;
        evEn->y = bitmapOp->operation.lo_y;
        evEn->width = width;
        evEn->height = height;
        evEn->callback = element->ge_img_button.ge_clickCallback;
        evEn->element = element;

        gfx_insert_ev(evEn);
    }
}

void gfx_handle_element(const struct GuiElement* element, Context* context) {
    if(!gfx_onlyDirty || element->ge_dirty || element->ge_type == GFX_BOX || context->c_forceRender) {
        switch(element->ge_type) {
            HANDLE_TYPE(GFX_BOX);
            HANDLE_TYPE(GFX_TEXT);
            HANDLE_TYPE(GFX_BUTTON);
            HANDLE_TYPE(GFX_BORDER);
            HANDLE_TYPE(GFX_IMAGE_BUTTON);
        }
    }
}

GfxRenderChain gfx_make_chain(const struct GuiElement* elements, size_t element_count) {
    // Create a base context
    Context ctx;
    ctx.c_x = 0;
    ctx.c_y = 0;
    ctx.c_prevWidth = TFT_WIDTH;
    ctx.c_prevHeight = TFT_HEIGHT;
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

GfxRenderChain gfx_create_render_chain(const struct GuiElement* elements, size_t elementCount) {
    gfx_onlyDirty = 0;
    return gfx_make_chain(elements, elementCount);
}

GfxRenderChain gfx_create_update_chain(const struct GuiElement* elements, size_t elementCount) {
    gfx_onlyDirty = 1;
    return gfx_make_chain(elements, elementCount);
}

void gfx_delete_render_chain(GfxRenderChain chain) {
    free(chain.grc_operations);

    struct EventListEntry* event = chain.grc_eventList;
    if(gfx_eventList == event)
        gfx_eventList = 0;

    while(event) {
        struct EventListEntry* next = event->next;
        free(event);
        event = next;
    }
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
