#include "st7789.h"


void app_main(void) {
    INIT();
    vTaskDelay(10);
    backlight(255/3);
    vTaskDelay(10);
    uint16_t makima = rgb888_to_rgb565(210, 132, 120);
    //fill_screen(makima);
    fill_screen(makima);
    vTaskDelay(10);
}
