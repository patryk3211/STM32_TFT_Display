#include <Arduino.h>

#include "lcd/tft_driver.h"
#include "lcd/freesans9pt7b.h"

#include "lcd/gfx.h"

const auto DARK_BLUE = tft_colmul(TFT_BLUE, 0.5);

GuiElement box1_children[] = {
    { GFX_TEXT, 0, 0, 0, 0, TFT_WHITE, .ge_font = &FreeSans9pt7b, .ge_text = "Header text" },
    { GFX_BUTTON, 10, 30, 50, 24, TFT_RED, TFT_WHITE, .ge_font = &FreeSans9pt7b, .ge_text = "[OK]",
        .ge_click_cb = [](const void*) { Serial.println("Clicked"); } }
};

GuiElement main_container_children[] = {
    { GFX_BOX, 20, 20, 280, 440, TFT_BLACK,
        .ge_childrenCount = 2, .ge_children = box1_children }
};

GuiElement main_element = {
    GFX_BOX, 0, 0, 320, 480, TFT_GREEN,
    .ge_childrenCount = 1,
    .ge_children = main_container_children
};

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
