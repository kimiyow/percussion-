#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

// pines del MUX
#define MUX_S0 GPIO_NUM_32
#define MUX_S1 GPIO_NUM_33
#define MUX_S2 GPIO_NUM_25
#define MUX_S3 GPIO_NUM_26
#define ADC_CHANNEL ADC_CHANNEL_6

#define NUM_CANALES 10

void app_main(void)
{
    const gpio_num_t mux_pines[] = { MUX_S0, MUX_S1, MUX_S2, MUX_S3 };
    for (int p = 0; p < 4; p++) {
        gpio_reset_pin(mux_pines[p]);
        gpio_set_direction(mux_pines[p], GPIO_MODE_OUTPUT);
        gpio_set_level(mux_pines[p], 0); // todos en LOW al inicio
    }

    // ADC 
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten    = ADC_ATTEN_DB_12
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));

    printf("=== SCAN MUX — 10 canales ===\n");
    printf("Formato: C0 C1 C2 C3 C4 C5 C6 C7 C8 C9\n\n");

    while (1) {
        int valores[NUM_CANALES] = {0};

        for (int i = 0; i < NUM_CANALES; i++) {
            gpio_set_level(MUX_S0, (i & 0x01) ? 1 : 0);
            gpio_set_level(MUX_S1, (i & 0x02) ? 1 : 0);
            gpio_set_level(MUX_S2, (i & 0x04) ? 1 : 0);
            gpio_set_level(MUX_S3, (i & 0x08) ? 1 : 0);

            esp_rom_delay_us(50); 

            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &valores[i]));
        }

        for (int i = 0; i < NUM_CANALES; i++) {
            printf("C%d:%d ", i, valores[i]);
        }
        printf("\n");

        vTaskDelay(pdMS_TO_TICKS(100)); // 10 lecturas por segundo
    }
}
