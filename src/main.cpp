#include <Arduino.h>

#include "tft_driver.h"

void setup() {
    tft_driver_init();

    LcdOperation* oper1 = tft_new_operation(RECT_FILL);
    oper1->lo_color.word = 0b0000000000011111;
    oper1->lo_x = 0;
    oper1->lo_y = 0;
    oper1->lo_width = 320;
    oper1->lo_height = 480;
    tft_submit(oper1);

    LcdOperation* oper2 = tft_new_operation(RECT_FILL);
    oper2->lo_color.word = 0b0111110000000000;
    oper2->lo_x = 40;
    oper2->lo_y = 40;
    oper2->lo_width = 40;
    oper2->lo_height = 40;
    tft_submit(oper2);

    LcdOperation* oper3 = tft_new_operation(RECT_FILL);
    oper3->lo_color.word = 0b0000001111100000;
    oper3->lo_x = 80;
    oper3->lo_y = 60;
    oper3->lo_width = 40;
    oper3->lo_height = 120;
    tft_submit(oper3);

    tft_start_render();
}

void loop() {

}
