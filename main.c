#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_adc/adc_oneshot.h"

//==================================================
// DEFINES
//==================================================

#define LED_GPIO        GPIO_NUM_32
#define BUTTON_GPIO     GPIO_NUM_35
#define ADC_CHANNEL     ADC_CHANNEL_6

//==================================================
// VARIABLES GLOBALES (REQUERIDAS)
//==================================================

volatile bool g_botonPres = false;
volatile bool g_sensorActivo = false;

typedef enum {
    ESTADO_RAPIDO,
    ESTADO_LENTO
} estado_t;

volatile estado_t g_estado = ESTADO_RAPIDO;

//==================================================
// HANDLES
//==================================================

TaskHandle_t xLedRapidoHandle = NULL;
TaskHandle_t xLedLentoHandle  = NULL;

//==================================================
// ADC
//==================================================

adc_oneshot_unit_handle_t adc_handle;

//==================================================
// GPIO INIT
//==================================================

void init_gpio(void)
{
    gpio_config_t led_cfg =
    {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_cfg);

    gpio_config_t btn_cfg =
    {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_cfg);
}

//==================================================
// ADC INIT
//==================================================

void init_adc(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg =
    {
        .unit_id = ADC_UNIT_1
    };

    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg =
    {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12
    };

    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_cfg);
}

//==================================================
// LED RAPIDO
//==================================================

void vTaskLedRapido(void *pv)
{
    while(1)
    {
        if (g_estado == ESTADO_RAPIDO)
        {
            gpio_set_level(LED_GPIO, 1);
            printf("[LED_R]\n");
            vTaskDelay(pdMS_TO_TICKS(100));

            gpio_set_level(LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

//==================================================
// LED LENTO + TIMEOUT
//==================================================

void vTaskLedLento(void *pv)
{
    TickType_t inicio;

    while(1)
    {
        if (g_estado == ESTADO_LENTO)
        {
            inicio = xTaskGetTickCount();

            while (g_estado == ESTADO_LENTO)
            {
                gpio_set_level(LED_GPIO, 1);
                printf("[LED_L]\n");
                vTaskDelay(pdMS_TO_TICKS(500));

                gpio_set_level(LED_GPIO, 0);
                vTaskDelay(pdMS_TO_TICKS(500));

                if ((xTaskGetTickCount() - inicio) >= pdMS_TO_TICKS(5000))
                {
                    printf("[LED_L] Timeout 5s -> RAPIDO\n");

                    g_estado = ESTADO_RAPIDO;
                    g_sensorActivo = false;

                    vTaskResume(xLedRapidoHandle);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

//==================================================
// SENSOR (SOLO EN LENTO)
//==================================================

void vTaskSensor(void *pv)
{
    int adc_raw;

    while(1)
    {
        if (g_estado == ESTADO_LENTO)
        {
            adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw);

            float v = (adc_raw * 3.3f) / 4095.0f;

            printf("[SENS] ADC=%d V=%.2f\n", adc_raw, v);
        }

        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

//==================================================
// MONITOR + BOTÓN (FLANCO)
//==================================================

void vTaskMonitor(void *pv)
{
    int last = 0;
    int actual;

    while(1)
    {
        actual = gpio_get_level(BUTTON_GPIO);

        if (actual == 1 && last == 0 && g_estado == ESTADO_RAPIDO)
        {
            printf("[MON] BOTON\n");

            g_botonPres = true;
            g_sensorActivo = true;
            g_estado = ESTADO_LENTO;

            vTaskSuspend(xLedRapidoHandle);
            vTaskResume(xLedLentoHandle);
        }

        last = actual;

        printf("[MON] Heap=%lu\n", esp_get_free_heap_size());
        printf("[MON] Stack=%u\n", uxTaskGetStackHighWaterMark(NULL));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//==================================================
// IDLE HOOK
//==================================================

void vApplicationIdleHook(void)
{
    static int c = 0;
    c++;

    if (c > 200000)
    {
        printf("[IDLE] CPU libre\n");
        c = 0;
    }
}

//==================================================
// APP MAIN
//==================================================

void app_main(void)
{
    init_gpio();
    init_adc();

    xTaskCreate(vTaskLedRapido, "RAPIDO", 2048, NULL, 1, &xLedRapidoHandle);
    xTaskCreate(vTaskLedLento,  "LENTO",  2048, NULL, 2, &xLedLentoHandle);
    xTaskCreate(vTaskSensor,    "SENS",   4096, NULL, 3, NULL);
    xTaskCreate(vTaskMonitor,   "MON",    4096, NULL, 4, NULL);

    vTaskSuspend(xLedLentoHandle);

    printf("=== SISTEMA INICIADO ===\n");
}