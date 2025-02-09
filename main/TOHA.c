#include "st7789.h"

void app_main(void) {
    INIT();
    vTaskDelay(pdMS_TO_TICKS(10)); // Espera para estabilizar

    backlight(255);  // Sube el brillo al máximo
    vTaskDelay(pdMS_TO_TICKS(10));

    // Dibuja un píxel visible
    draw_pixel(100, 100, rgb888_to_rgb565(208, 0, 0)); // Rojo

    // Rellena la pantalla con un color visible
    draw_rectangle(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1, rgb888_to_rgb565(207, 130, 122));
    vTaskDelay(10);
    draw_rectangle(0,0,100, 100, rgb888_to_rgb565(100, 100, 100));
}
