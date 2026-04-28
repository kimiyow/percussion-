#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"

// Definimos el canal ADC que corresponde al GPIO 34
#define PIEZO_ADC_CHANNEL ADC_CHANNEL_6 

void app_main(void)
{
    // Inicilizador del ADC en modo One-Shot (lectura puntual, no continua)
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    // Creamos la instancia del ADC1
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, 
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, PIEZO_ADC_CHANNEL, &config));

    int adc_raw = 0;

    
    while (1) {
        // Lectura cruda del sensor
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, PIEZO_ADC_CHANNEL, &adc_raw));

        // Umbral para filtrar el ruido de fond
        if (adc_raw > 100) {
          
            printf("%d\n", adc_raw);
            vTaskDelay(pdMS_TO_TICKS(50)); 
        } else {
            printf("0\n");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}