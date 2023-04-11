#include <Arduino.h>

#include "tft_driver.h"
#include "freesans9pt7b.h"

void setup() {
    Serial.begin(9600);

    tft_driver_init();

    LcdOperation* oper1 = tft_new_operation(RECT_FILL);
    oper1->lo_fg.word = 0;
    oper1->lo_x = 0;
    oper1->lo_y = 0;
    oper1->lo_rect.width = 320;
    oper1->lo_rect.height = 480;
    tft_submit(oper1);

    LcdOperation* textOper = tft_new_operation(TEXT);
    textOper->lo_fg.word = 0b111110000000000;
    textOper->lo_x = 20;
    textOper->lo_y = 20;
    //textOper->lo_rect.width = 50;
    //textOper->lo_rect.height = 50;
    textOper->lo_text.font = &FreeSans9pt7b;
    textOper->lo_text.value = "Text Text\nWow a new line\nShort";
    tft_submit(textOper);

    tft_start_render();
}

void loop() {

}
