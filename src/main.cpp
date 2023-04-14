#include <Arduino.h>

#include "tft_driver.h"
#include "freesans9pt7b.h"

LcdOperation operations[] = {
    { RECT_FILL, TFT_BLACK, TFT_BLACK, 1, 0, 0, 0, { 320, 480 } },
    { TEXT, TFT_WHITE, tft_colmul(TFT_GREEN, 0.25), 1, 0, 20, 20, .lo_text = { "Test Text", &FreeSans9pt7b } },
    { RECT_FILL, TFT_GREEN, TFT_BLACK, 1, 0, 30, 100, { 50, 50 } }
};

void setup() {
    Serial.begin(9600);

    tft_driver_init();

    tft_submit_multiple(operations, 3);

    tft_start_render();
}

void loop() {

}
