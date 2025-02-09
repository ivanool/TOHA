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

void send_data16(uint16_t data){
    gpio_set_level(TFT_DC, DATA_MODE);
    uint8_t buffer[2] = {data >> 8, data & 0xFF};
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = buffer
    };
    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));
}

void send_data8(uint8_t data) {
    gpio_set_level(TFT_DC, DATA_MODE);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data
    };
    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));
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
    vTaskDelay(pdMS_TO_TICKS(150));
}

void porch_control(uint8_t bpa, uint8_t fpa, bool psen, uint8_t bpb, uint8_t fpb, uint8_t bpc, uint8_t fpc) {
    send_cmd(PORCTRL);
    send_data8(bpa & 0x7F);
    send_data8(fpa & 0x7F);
    uint8_t byte3 = (psen ? 0x80 : 0x00) | (bpb & 0x0F);
    send_data8(byte3);
    send_data8(fpb & 0x0F);
    send_data8(bpc & 0x0F);
    send_data8(fpc & 0x0F);
}

void set_orientation(uint8_t data){
    send_cmd(MADCTL);
    send_data8(data);
}

void set_window(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1) {
    send_cmd(CASET);
    send_data16(x0);
    send_data16(x1);
    send_cmd(RASET);
    send_data16(y0);
    send_data16(y1);
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
    send_data8(0x55);
    set_orientation(0xC0);
    porch_control(0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01);
    send_cmd(GCTRL);
    send_data8(0x35);
    send_cmd(VCOMS);
    send_data8(0x1F);
    send_cmd(INVON);
    vTaskDelay(pdMS_TO_TICKS(10));
    send_cmd(DISPON);
    vTaskDelay(pdMS_TO_TICKS(150));
    send_cmd(TEON);
    send_data8(0x00);
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

void draw_pixel(uint16_t x, uint16_t y, uint16_t color){
    set_window(x, x, y, y);
    send_cmd(RAMWR);
    send_data16(color);
}

void draw_row(uint16_t y, uint16_t *pixels){
    uint8_t pixel_buffer[TFT_WIDTH * 2];
    set_window(0, TFT_WIDTH - 1, y, y);
    for(int i = 0; i < TFT_WIDTH; i++){
        pixel_buffer[i * 2] = (pixels[i] >> 8) & 0xFF;
        pixel_buffer[i * 2 + 1] = pixels[i] & 0xFF;
    }
    send_cmd(RAMWR);
    gpio_set_level(TFT_DC, DATA_MODE);
    spi_transaction_t t = {
        .length = TFT_WIDTH * 16,
        .tx_buffer = pixel_buffer
    };
    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));
}

void clear_screen(uint16_t color){
    uint16_t color_row[TFT_WIDTH];
    for(int i = 0; i < TFT_WIDTH; i++){
        color_row[i] = color;
    }
    for(uint16_t i = 0; i < TFT_HEIGHT; i++){
        draw_row(i, color_row);
    }
}

void fill_screen(uint16_t color){
    uint16_t *page = (uint16_t *)heap_caps_malloc(TFT_WIDTH * TFT_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!page) {
        printf("Error: No se pudo asignar memoria para el buffer\n");
        return;
    }

    for(int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
        page[i] = (color >> 8) | (color << 8);
    }

    set_window(0, TFT_WIDTH-1, 0, TFT_HEIGHT-1);
    send_cmd(RAMWR);
    gpio_set_level(TFT_DC, DATA_MODE);

    spi_transaction_t t = {
        .length = TFT_HEIGHT * TFT_WIDTH * 16,
        .tx_buffer = page
    };

    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));
    heap_caps_free(page);
    
}


void makima(){
    return;
}