#include <Arduino.h>

#include "lcd/tft_driver.h"
#include "lcd/fonts/freesans9pt7b.h"
#include "lcd/fonts/freesans12pt7b.h"
#include "lcd/fonts/freemonobold12pt7b.h"

#include "lcd/gfx.h"

#define LIGHT_BLUE TFT_COLOR(15, 15, 31)
#define DARK_GRAY TFT_COLOR(1, 1, 1)

const GuiElement header_children[] = {
    GUI_TEXT(6, 2, TFT_WHITE, "CNC Controller v1.0.0", FreeSans12pt7b)
};

GuiElement position_box_children[] = {
    GUI_BORDER(-1, -1, PARENT_WIDTH(1), PARENT_HEIGHT(1), TFT_CYAN, 1),

    GUI_TEXT(4, 4, TFT_WHITE, "Position", FreeSans12pt7b),
    GUI_TEXT_RIGHT(PARENT_WIDTH(-4), 4, TFT_WHITE, "[mm]", FreeSans12pt7b),

    GUI_TEXT(8, 8 + 24 * 1, TFT_RED,   "X: 000.00 000.00", FreeMonoBold12pt7b),
    GUI_TEXT(8, 8 + 24 * 2, TFT_GREEN, "Y: 000.00 000.00", FreeMonoBold12pt7b),
    GUI_TEXT(8, 8 + 24 * 3, TFT_BLUE,  "Z: 000.00 000.00", FreeMonoBold12pt7b)
};

const GuiElement me_children[] = {
    GUI_BOX_STATIC(0, 0, PARENT_WIDTH(0), 32, TFT_BLUE, header_children),

    GUI_BOX_STATIC(16, 48, PARENT_WIDTH(-32), 112, TFT_BLACK, position_box_children),

    GUI_BUTTON( 16, 440, 80, 24, TFT_BLUE, TFT_WHITE, "[Zero]", FreeSans9pt7b, 0),
    GUI_BUTTON(120, 440, 80, 24, TFT_BLUE, TFT_WHITE, "[Menu]", FreeSans9pt7b, 0),
    GUI_BUTTON(224, 440, 80, 24, TFT_BLUE, TFT_WHITE, "[Home]", FreeSans9pt7b, 0)
};

const GuiElement main_element = GUI_BOX_STATIC(0, 0, 320, 480, TFT_BLACK, me_children);

extern uint16_t tft_tpX;
extern uint16_t tft_tpY;

void setup() {
    Serial.begin(9600);

    tft_driver_init();

    auto result = gfx_create_render_chain(&main_element, 1);
    tft_submit_multiple(result.grc_operations, result.grc_length);

    gfx_activate_event_list(result.grc_eventList);

    tft_start_render();

    attachInterrupt(PB12, &tft_tp_irq, FALLING);
}

void loop() {
    tft_main_loop();
}
