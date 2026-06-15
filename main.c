/*
 * Este programa implementa un sistema multitarea en un ESP32 utilizando FreeRTOS.
 * El sistema inicia con un LED parpadeando en modo rapido. Al detectar la
 * pulsacion de un botón, cambia a un modo lento donde el LED reduce su frecuencia
 * de parpadeo y se habilita la lectura de un sensor analogico mediante el ADC.
 * Despues de 5 segundos en este estado, el sistema regresa automaticamente al
 * modo rapido. Adicionalmente, se monitorea el uso de memoria y la actividad
 * del procesador mediante tareas de supervision e Idle Hook.
 */

#include <stdio.h>
#include <stdbool.h>

/* librerias de FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* librerias del ESP32 */
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_adc/adc_oneshot.h"

//defines

/* Pin del LED */
#define LED_GPIO        GPIO_NUM_32

/* Pin del boton */
#define BUTTON_GPIO     GPIO_NUM_35

/* Canal ADC utilizado para leer el sensor */
#define ADC_CHANNEL     ADC_CHANNEL_6

//==================================================
// VARIABLES GLOBALES
//==================================================

/* boton presionado */
volatile bool g_botonPres = false;

/* sesnor activo */
volatile bool g_sensorActivo = false;

/* Estados posibles del sistema */
typedef enum {
    ESTADO_RAPIDO,   // LED parpadea rapido
    ESTADO_LENTO     // LED parpadea lento y se activa el sensor
} estado_t;

/* Estado inicial del sistema */
volatile estado_t g_estado = ESTADO_RAPIDO;

//==================================================
// HANDLES DE TAREAS
//==================================================

/* Handles para poder suspender o reanudar tareas */
TaskHandle_t xLedRapidoHandle = NULL;
TaskHandle_t xLedLentoHandle  = NULL;

//==================================================
// ADC
//==================================================

/* Handle del ADC */
adc_oneshot_unit_handle_t adc_handle;

//==================================================
// GPIO INIT
//==================================================

void init_gpio(void)
{
    /* Configuracion del LED como salida */
    gpio_config_t led_cfg =
    {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&led_cfg);

    /* Configuracion del boton como entrada */
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
    /* Configuracion de la unidad ADC1 */
    adc_oneshot_unit_init_cfg_t init_cfg =
    {
        .unit_id = ADC_UNIT_1
    };

    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    /* Configuracion del canal ADC */
    adc_oneshot_chan_cfg_t chan_cfg =
    {
        .bitwidth = ADC_BITWIDTH_12,  // Resolucion de 12 bits: 0 a 4095
        .atten = ADC_ATTEN_DB_12      // Atenuacion para ampliar el rango de lectura
    };

    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_cfg);
}

//==================================================
// TAREA LED RÁPIDO
//==================================================

void vTaskLedRapido(void *pv)
{
    while(1)
    {
        /* Solo parpadea si el sistema está en estado rapido */
        if (g_estado == ESTADO_RAPIDO)
        {
            gpio_set_level(LED_GPIO, 1);      // Enciende LED
            printf("[LED_R]\n");             // Mensaje de depuracion
            vTaskDelay(pdMS_TO_TICKS(100));  // Espera 100 ms

            gpio_set_level(LED_GPIO, 0);      // Apaga LED
            vTaskDelay(pdMS_TO_TICKS(100));  // Espera 100 ms
        }
        else
        {
            /* Si no esta en modo rápido, espera para no saturar la CPU */
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

//==================================================
// TAREA LED LENTO + TIMEOUT
//==================================================

void vTaskLedLento(void *pv)
{
    TickType_t inicio;

    while(1)
    {
        /* Solo funciona si el sistema esta en estado lento */
        if (g_estado == ESTADO_LENTO)
        {
            /* Guarda el tiempo en el que inició el modo lento */
            inicio = xTaskGetTickCount();

            while (g_estado == ESTADO_LENTO)
            {
                gpio_set_level(LED_GPIO, 1);      // Enciende LED
                printf("[LED_L]\n");
                vTaskDelay(pdMS_TO_TICKS(500));  // Espera 500 ms

                gpio_set_level(LED_GPIO, 0);      // Apaga LED
                vTaskDelay(pdMS_TO_TICKS(500));  // Espera 500 ms

                /* Si pasan 5 segundos en modo lento, regresa a modo rapido */
                if ((xTaskGetTickCount() - inicio) >= pdMS_TO_TICKS(5000))
                {
                    printf("[LED_L] Timeout 5s -> RAPIDO\n");

                    g_estado = ESTADO_RAPIDO;    // Cambia al estado rápido
                    g_sensorActivo = false;      // Desactiva bandera del sensor

                    vTaskResume(xLedRapidoHandle); // Reanuda la tarea rápida
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

//==================================================
// TAREA SENSOR
//==================================================

void vTaskSensor(void *pv)
{
    int adc_raw;

    while(1)
    {
        /* El sensor solo se lee cuando el sistema está en modo lento */
        if (g_estado == ESTADO_LENTO)
        {
            /* Lectura cruda del ADC */
            adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw);

            /* Conversión aproximada de ADC a voltaje */
            float v = (adc_raw * 3.3f) / 4095.0f;

            printf("[SENS] ADC=%d V=%.2f\n", adc_raw, v);
        }

        /* Espera 300 ms entre lecturas */
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

//==================================================
// TAREA MONITOR + BOTON
//==================================================

void vTaskMonitor(void *pv)
{
    int last = 0;
    int actual;

    while(1)
    {
        /* Lee el estado actual del boton */
        actual = gpio_get_level(BUTTON_GPIO);

        /*
           Detecta flanco de subida:
           actual == 1: boton presionado
           last == 0: antes no estaba presionado
           g_estado == ESTADO_RAPIDO: solo cambia si esta en modo rápido
        */
        if (actual == 1 && last == 0 && g_estado == ESTADO_RAPIDO)
        {
            printf("[MON] BOTON\n");

            g_botonPres = true;        // Guarda que el boton fue presionado
            g_sensorActivo = true;     // Activa bandera del sensor
            g_estado = ESTADO_LENTO;   // Cambia a estado lento

            vTaskSuspend(xLedRapidoHandle); // Suspende tarea rapida
            vTaskResume(xLedLentoHandle);   // Reanuda tarea lenta
        }

        /* Guarda la lectura anterior del boton */
        last = actual;

        /* Imprime info del sistema */
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

    /*
       Esta funcion se ejecuta cuando no hay otra tarea lista.
       Sirve para indicar que la CPU tiene tiempo libre.
    */
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
    /* Inicializa perifericos */
    init_gpio();
    init_adc();

    /* Crea las tareas del sistema */
    xTaskCreate(vTaskLedRapido, "RAPIDO", 2048, NULL, 1, &xLedRapidoHandle);
    xTaskCreate(vTaskLedLento,  "LENTO",  2048, NULL, 2, &xLedLentoHandle);
    xTaskCreate(vTaskSensor,    "SENS",   4096, NULL, 3, NULL);
    xTaskCreate(vTaskMonitor,   "MON",    4096, NULL, 4, NULL);

    /* Al inicio solo debe funcionar el LED rapido */
    vTaskSuspend(xLedLentoHandle);

    printf("=== SISTEMA INICIADO ===\n");
}
