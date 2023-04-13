#include <Arduino.h>

#include "tft_driver.h"
#include "freesans9pt7b.h"

LcdOperation operations[] = {
    { RECT_FILL, TFT_BLACK, TFT_BLACK, 1, 0, 0, 0, { 320, 480 } },
    { TEXT, TFT_WHITE, TFT_GREEN, 1, 0, 20, 20, .lo_text = { "Test Text", &FreeSans9pt7b } },
    { RECT_FILL, TFT_GREEN, TFT_BLACK, 1, 0, 30, 100, { 50, 50 } }
};

void setup() {
    Serial.begin(9600);

    tft_driver_init();

    /*LcdOperation* fillScreen = tft_new_operation(RECT_FILL);
    fillScreen->lo_fg = TFT_BLACK;
    fillScreen->lo_x = 0;
    fillScreen->lo_y = 0;
    fillScreen->lo_rect.width = 320;
    fillScreen->lo_rect.height = 480;
    tft_submit(fillScreen);

    LcdOperation* textOper = tft_new_operation(TEXT);
    textOper->lo_fg = TFT_WHITE;
    textOper->lo_bg = tft_colmul(TFT_GREEN, 0.25);
    textOper->lo_x = 20;
    textOper->lo_y = 20;
    textOper->lo_text.font = &FreeSans9pt7b;
    textOper->lo_text.value = "Text Text\nWow a new line\nShort";
    tft_submit(textOper);

    LcdOperation* smallRect = tft_new_operation(RECT_FILL);
    smallRect->lo_fg = TFT_GREEN;
    smallRect->lo_x = 30;
    smallRect->lo_y = 100;
    smallRect->lo_rect.width = 50;
    smallRect->lo_rect.height = 70;
    tft_submit(smallRect);*/
    tft_submit_multiple(operations, 3);

    tft_start_render();
}

void loop() {

}
