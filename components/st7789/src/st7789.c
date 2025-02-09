#include "st7789.h"




spi_device_handle_t spi;




void send_cmd(uint8_t cmd) {
    gpio_set_level(TFT_DC, CMD_MODE);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd
    };
    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));
}

void send_data(const uint8_t* data, size_t size) {
    spi_transaction_t SPIT;
    gpio_set_level(TFT_DC, DATA_MODE);
    memset(&SPIT, 0, sizeof(spi_transaction_t));
    SPIT.length = size * 8;
    SPIT.tx_buffer = data;
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &SPIT)); // Usar solo una transmisión
}

void send_word(uint16_t data){
    uint8_t buffer[2] = {data >> 8, data & 0xFF};
    // uint8_t buffer[2] = {data >> 8, data & 0xFF}; // Unused variable removed
    send_data((const uint8_t*)buffer, 2);
}


void backlight(uint8_t duty) {

    ledc_timer_config_t ledc_timer = {
        .speed_mode = SPEED_MODE,
        .timer_num = TIMER_NUM,
        .duty_resolution = DUTY_RESOLUTION,
        .freq_hz = FREQUENCY_TIMER,
        .clk_cfg = CLK_CFG,
    };

    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode = SPEED_MODE,
        .channel = LEDC_CHANNEL,
        .gpio_num = GPIO_NUM,
        .timer_sel = TIMER_NUM,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ledc_channel);

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL);

}


void RESET(){
    gpio_set_level(TFT_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(TFT_RST, 1);
    send_cmd(SWRESET);

    vTaskDelay(pdMS_TO_TICKS(150));
}

void porch_control(uint8_t bpa, uint8_t fpa, bool psen, uint8_t bpb, uint8_t fpb, uint8_t bpc, uint8_t fpc) {
    send_cmd(PORCTRL);
    uint8_t bpa_data = bpa & 0x7F;
    send_data(&bpa_data, 1);
    uint8_t fpa_data = fpa & 0x7F;
    send_data(&fpa_data, 1);
    uint8_t byte3 = (psen ? 0x80 : 0x00) | (bpb & 0x0F);
    send_data(&byte3, 1);
    uint8_t fpb_data = fpb & 0x0F;
    send_data(&fpb_data, 1);
    uint8_t bpc_data = bpc & 0x0F;
    send_data(&bpc_data, 1);
    uint8_t fpc_data = fpc & 0x0F;
    send_data(&fpc_data, 1);
}

void set_orientation(uint8_t data){
    send_cmd(MADCTL);
    send_data(&data, 1);
}

#define X_OFFSET 0  // O el valor que requiera tu módulo
#define Y_OFFSET 0  // O el valor que requiera tu módulo

void set_window(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1) {
    send_cmd(CASET);
    uint16_t x_start = x0 + X_OFFSET;
    uint16_t x_end   = x1 + X_OFFSET;
    send_word(x_start);
    send_word(x_end);
    send_cmd(RASET);
    uint16_t y_start = y0 + Y_OFFSET;
    uint16_t y_end   = y1 + Y_OFFSET;
    send_word(y_start);
    send_word(y_end);
}


void gpio_init() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TFT_DC) | (1ULL << TFT_RST) | (1ULL << TFT_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

void INIT() {
    spi_init();
    gpio_init();
    RESET();
    send_cmd(SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));
    send_cmd(COLMOD);
    uint8_t colmod_data = 0x55;
    send_data(&colmod_data, 1);
    // En INIT() cambiar orientación a 0x00 (prueba diferentes valores)
    set_orientation(0x00); 

// Verificar parámetros del porche según datasheet
    porch_control(0x0C, 0x0C, 0x00, 0x10, 0x10, 0x02, 0x02); // Valores típicos
    send_cmd(GCTRL);
    uint8_t gctrl_data = 0x35;
    send_data(&gctrl_data, 1);
    send_cmd(VCOMS);
    uint8_t vcoms_data = 0x1F;
    send_data(&vcoms_data, 1);
    send_cmd(INVON);
    vTaskDelay(pdMS_TO_TICKS(10));
    send_cmd(NORON);
    send_cmd(DISPON);
    vTaskDelay(pdMS_TO_TICKS(150));
    send_cmd(TEON);
    uint8_t teon = 0x00;
    send_data(&teon, 1);
    backlight(10);
}

void spi_init() {
    spi_bus_config_t buscfg = {
        .mosi_io_num = TFT_MOSI,
        .sclk_io_num = TFT_SCLK,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_HEIGHT * TFT_WIDTH * 16
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SPI_FREQUENCY,
        .mode = 0,
        .spics_io_num = TFT_CS,
        .queue_size = 9,
        .flags = SPI_DEVICE_NO_DUMMY
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi));
}

uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}


void send_color(uint16_t * color, uint16_t size){
    static uint8_t byte[1024];
    int index = 0;
    for(int i = 0; i < size; i++){
        byte[index++] = (color[i]>>8) & 0xFF;
        byte[index++] = color[i]&0xFF;
    }
    send_data(byte, size*2);
}

void draw_pixel(uint16_t x, uint16_t y, uint16_t color){
    set_window(x, x, y, y);
    send_cmd(RAMWR);
    send_color(&color, 1);
}

void draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    set_window(x1, x2, y1, y2);
    send_cmd(RAMWR);

    uint16_t width = x2 - x1 + 1;
    uint16_t height = y2 - y1 + 1;
    uint32_t total_pixels = width * height;
    uint8_t buffer[1024];
    
    // Llenar buffer con el color repetido
    for (int i = 0; i < sizeof(buffer)/2; i++) {
        buffer[i*2] = (color >> 8) & 0xFF;
        buffer[i*2 + 1] = color & 0xFF;
    }

    uint32_t sent = 0;
    while (sent < total_pixels) {
        uint32_t remaining = total_pixels - sent;
        uint32_t chunk = (remaining > sizeof(buffer)/2) ? sizeof(buffer)/2 : remaining;
        send_data(buffer, chunk * 2);
        sent += chunk;
    }
}