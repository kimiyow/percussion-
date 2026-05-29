#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"


#define MUX_S0 GPIO_NUM_32
#define MUX_S1 GPIO_NUM_33
#define MUX_S2 GPIO_NUM_25
#define MUX_S3 GPIO_NUM_26
#define ADC_CHANNEL ADC_CHANNEL_6

#define NUM_CANALES 10
#define MUESTRAS_ADC             3
#define DELAY_ENTRE_MUESTRAS_US 20
#define DELAY_ESTABILIZACION_US 50

// umbral:      es el valor minimo para considerar golpe válido (filtro de ruido)
// umbral_reset: es el valor en que la señal debe bajar antes del siguiente golpe
// ventana_peak: ms buscando el pico maximo tras superar el umbral
// cooldown:    ms minimos entre golpes del mismo canal


typedef struct {
    const char* nombre;
    int umbral;
    int umbral_reset;
    int ventana_peak;
    int cooldown;
} InstrumentoConfig;

const InstrumentoConfig perfiles[NUM_CANALES] = {

    { .nombre="Bombo",  .umbral=200, .umbral_reset=80, .ventana_peak=35, .cooldown=100 },
    { .nombre="Tom",    .umbral=100, .umbral_reset=80, .ventana_peak=12, .cooldown=30  },
    { .nombre="Tumba",  .umbral=150, .umbral_reset=60, .ventana_peak=12, .cooldown=22  },
    { .nombre="Bongo1", .umbral=80,  .umbral_reset=30, .ventana_peak=5,  .cooldown=10  },
    { .nombre="Bongo2", .umbral=80,  .umbral_reset=30, .ventana_peak=5,  .cooldown=10  },
    { .nombre="WB1",    .umbral=120, .umbral_reset=48, .ventana_peak=6,  .cooldown=12  },
    { .nombre="WB2",    .umbral=120, .umbral_reset=48, .ventana_peak=6,  .cooldown=12  },
    { .nombre="WB3",    .umbral=120, .umbral_reset=48, .ventana_peak=6,  .cooldown=12  },
    { .nombre="WB4",    .umbral=120, .umbral_reset=48, .ventana_peak=6,  .cooldown=12  },
    { .nombre="WB5",    .umbral=120, .umbral_reset=48, .ventana_peak=6,  .cooldown=12  },
};

static int64_t tiempo_ultimo_golpe[NUM_CANALES]   = {0};
static int64_t tiempo_inicio_ventana[NUM_CANALES] = {0};
static int     max_pico_detectado[NUM_CANALES]    = {0};
static bool    buscando_pico[NUM_CANALES]         = {false};
static bool    esperando_reset[NUM_CANALES]       = {false};

static inline int leer_adc_maximo(adc_oneshot_unit_handle_t handle)
{
    int maximo = 0;
    for (int m = 0; m < MUESTRAS_ADC; m++) {
        int tmp = 0;
        adc_oneshot_read(handle, ADC_CHANNEL, &tmp);
        if (tmp > maximo) maximo = tmp;
        if (m < MUESTRAS_ADC - 1) esp_rom_delay_us(DELAY_ENTRE_MUESTRAS_US);
    }
    return maximo;
}

void app_main(void)
{
    const gpio_num_t mux_pines[] = { MUX_S0, MUX_S1, MUX_S2, MUX_S3 };
    for (int p = 0; p < 4; p++) {
        gpio_reset_pin(mux_pines[p]);
        gpio_set_direction(mux_pines[p], GPIO_MODE_OUTPUT);
        gpio_set_level(mux_pines[p], 0);
    }

    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&init_config, &adc1_handle);

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten    = ADC_ATTEN_DB_12
    };
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config);

    printf("Sistema listo.\n");

    while (1) {
        int64_t ahora = esp_timer_get_time() / 1000;

        for (int i = 0; i < NUM_CANALES; i++) {
            gpio_set_level(MUX_S0, (i & 0x01) ? 1 : 0);
            gpio_set_level(MUX_S1, (i & 0x02) ? 1 : 0);
            gpio_set_level(MUX_S2, (i & 0x04) ? 1 : 0);
            gpio_set_level(MUX_S3, (i & 0x08) ? 1 : 0);

            esp_rom_delay_us(DELAY_ESTABILIZACION_US);
            int piezo_value = leer_adc_maximo(adc1_handle);

            if (esperando_reset[i]) {
                if (piezo_value < perfiles[i].umbral_reset)
                    esperando_reset[i] = false;
                continue;
            }

            if (!buscando_pico[i]) {
                if (piezo_value > perfiles[i].umbral &&
                   (ahora - tiempo_ultimo_golpe[i]) > perfiles[i].cooldown) {
                    buscando_pico[i]         = true;
                    tiempo_inicio_ventana[i] = ahora;
                    max_pico_detectado[i]    = piezo_value;
                }
            } else {
                if (piezo_value > max_pico_detectado[i])
                    max_pico_detectado[i] = piezo_value;

                if ((ahora - tiempo_inicio_ventana[i]) >= perfiles[i].ventana_peak) {
                    
                    printf("%d:%d\n", i, max_pico_detectado[i]);

                    buscando_pico[i]       = false;
                    tiempo_ultimo_golpe[i] = ahora;
                    max_pico_detectado[i]  = 0;
                    esperando_reset[i]     = true;
                }
            }
        }

        vTaskDelay(1);
    }
}
