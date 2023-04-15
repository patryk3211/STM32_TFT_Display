#ifndef MODULES_FONT_H
#define MODULES_FONT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct BitmapFontGlyph {
    uint16_t bfg_bitmapOffset;
    uint8_t bfg_width;
    uint8_t bfg_height;
    uint8_t bfg_xAdvance;
    int8_t bfg_xOffset;
    int8_t bfg_yOffset;
};

struct BitmapFont {
    uint8_t* bf_bitmap;
    struct BitmapFontGlyph* bf_glyphs;

    uint8_t bf_firstChar;
    uint8_t bf_lastChar;

    uint8_t bf_yAdvance;
};

#ifdef __cplusplus
}
#endif

#endif
