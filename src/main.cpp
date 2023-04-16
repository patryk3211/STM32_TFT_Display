#include <Arduino.h>

#include "lcd/tft_driver.h"
#include "lcd/fonts/freesans9pt7b.h"
#include "lcd/fonts/freesans12pt7b.h"
#include "lcd/fonts/freemonobold12pt7b.h"

#include "lcd/gfx.h"

#define LIGHT_BLUE TFT_COLOR(15, 15, 31)
#define DARK_GRAY TFT_COLOR(4, 4, 4)

const GuiElement header_children[] = {
    GUI_TEXT(6, 2, TFT_WHITE, "CNC Controller v1.0.0", FreeSans12pt7b)
};

GuiElement position_box_children[] = {
    GUI_BORDER(-1, -1, PARENT_WIDTH(2), PARENT_HEIGHT(2), TFT_CYAN, 1),

    GUI_TEXT(4, 4, TFT_WHITE, "Position", FreeSans12pt7b),
    GUI_TEXT_RIGHT(PARENT_WIDTH(-4), 4, TFT_WHITE, "[mm]", FreeSans12pt7b),

    GUI_TEXT(8, 8 + 24 * 1, TFT_RED,   "X: 000.00 000.00", FreeMonoBold12pt7b),
    GUI_TEXT(8, 8 + 24 * 2, TFT_GREEN, "Y: 000.00 000.00", FreeMonoBold12pt7b),
    GUI_TEXT(8, 8 + 24 * 3, TFT_BLUE,  "Z: 000.00 000.00", FreeMonoBold12pt7b)
};

const uint8_t button_up[] = {0, 0, 127, 254, 64, 2, 65, 130, 67, 194, 71, 226, 79, 242, 65, 130, 65, 130, 65, 130, 65, 130, 65, 130, 65, 130, 64, 2, 127, 254, 0, 0};
const uint8_t button_down[] = {0, 0, 127, 254, 64, 2, 65, 130, 65, 130, 65, 130, 65, 130, 65, 130, 65, 130, 79, 242, 71, 226, 67, 194, 65, 130, 64, 2, 127, 254, 0, 0};
const uint8_t button_left[] = {0, 0, 127, 254, 64, 2, 64, 2, 66, 2, 70, 2, 78, 2, 95, 250, 95, 250, 78, 2, 70, 2, 66, 2, 64, 2, 64, 2, 127, 254, 0, 0};
const uint8_t button_right[] = {0, 0, 127, 254, 64, 2, 64, 2, 64, 66, 64, 98, 64, 114, 95, 250, 95, 250, 64, 114, 64, 98, 64, 66, 64, 2, 64, 2, 127, 254, 0, 0};

GuiElement control_box_rates_children[] = {
    GUI_BUTTON(0, 0, 120, 24, TFT_YELLOW, TFT_BLACK, "Step: 1.00mm", FreeSans9pt7b, 0),
    GUI_BUTTON(148, 0, 120, 24, TFT_YELLOW, TFT_BLACK, "Feed: 100mm/s", FreeSans9pt7b, 0)
};

GuiElement control_box_children[] = {
    GUI_BORDER(-1, -1, PARENT_WIDTH(2), PARENT_HEIGHT(2), TFT_YELLOW, 1),

    GUI_BOX_STATIC(0, 0, PARENT_WIDTH(0), 24, TFT_YELLOW, control_box_rates_children),

    GUI_IMAGE_BUTTON( 60, 32, 48, 48, 3, TFT_COLOR(0, 1, 0), TFT_GREEN, button_up, 0, 0),
    GUI_IMAGE_BUTTON( 60, 88, 48, 48, 3, TFT_COLOR(0, 1, 0), TFT_GREEN, button_down, 0, 0),
    GUI_IMAGE_BUTTON(  8, 60, 48, 48, 3, TFT_COLOR(1, 0, 0), TFT_RED  , button_left, 0, 0),
    GUI_IMAGE_BUTTON(112, 60, 48, 48, 3, TFT_COLOR(1, 0, 0), TFT_RED  , button_right, 0, 0),

    GUI_IMAGE_BUTTON(192, 32, 48, 48, 3, TFT_COLOR(0, 0, 1), TFT_BLUE, button_up, 0, 0),
    GUI_IMAGE_BUTTON(192, 88, 48, 48, 3, TFT_COLOR(0, 0, 1), TFT_BLUE, button_down, 0, 0),
};

GuiElement alarm_box_children[] = {
    GUI_BORDER(-1, -1, PARENT_WIDTH(2), PARENT_HEIGHT(2), TFT_RED, 2),

    GUI_TEXT_CENTERED(0, 2, PARENT_WIDTH(0), TFT_RED, "Alarm", FreeSans12pt7b)
};

GuiElement me_children[] = {
    GUI_BOX_STATIC(0, 0, PARENT_WIDTH(0), 32, TFT_BLUE, header_children),

    GUI_BOX_STATIC(16, 48, PARENT_WIDTH(-32), 112, TFT_BLACK, position_box_children),
    GUI_BOX_STATIC(16, 176, PARENT_WIDTH(-32), 144, TFT_BLACK, control_box_children),

    GUI_BOX_STATIC(16, 336, PARENT_WIDTH(-32), 33, TFT_BLACK, alarm_box_children),

    GUI_BUTTON( 16, 440, 80, 24, TFT_BLUE, TFT_WHITE, "[Zero]", FreeSans9pt7b, 0),
    GUI_BUTTON(120, 440, 80, 24, TFT_BLUE, TFT_WHITE, "[Menu]", FreeSans9pt7b, 0),
    GUI_BUTTON(224, 440, 80, 24, TFT_BLUE, TFT_WHITE, "[Home]", FreeSans9pt7b, 0)
};

const GuiElement main_element = GUI_BOX_STATIC(0, 0, 320, 480, TFT_BLACK, me_children);

uint32_t prevUpdate = 0;

void setup() {
    Serial.begin(9600);

    tft_driver_init();

    auto result = gfx_create_render_chain(&main_element, 1);
    tft_submit_multiple(result.grc_operations, result.grc_length);

    gfx_activate_event_list(result.grc_eventList);

    tft_start_render();

    attachInterrupt(PB12, &tft_tp_irq, FALLING);

    prevUpdate = millis();
}

GfxRenderChain chain;
bool state = false;

extern "C" void tft_render_finished() {
    gfx_delete_render_chain(chain);
}

void loop() {
    tft_main_loop();

    if(millis() - prevUpdate > 2000) {
        if(!state) {
            alarm_box_children[1].ge_color = TFT_BLACK;
            me_children[3].ge_color = TFT_RED;
            state = true;
        } else {
            alarm_box_children[1].ge_color = TFT_RED;
            me_children[3].ge_color = TFT_BLACK;
            state = false;
        }
        me_children[3].ge_dirty = true;

        chain = gfx_create_update_chain(&main_element, 1);
        tft_submit_multiple(chain.grc_operations, chain.grc_length);
        tft_start_render();

        prevUpdate = millis();
    }
}
