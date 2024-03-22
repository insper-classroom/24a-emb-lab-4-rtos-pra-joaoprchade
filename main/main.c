/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

const int ECHO_PIN = 16;
const int TRIG_PIN = 15;

volatile bool timer_fired = false;

volatile int start_time = 0;
volatile int end_time = 0;

SemaphoreHandle_t xSemaphore_t;
QueueHandle_t xQueueTime;
QueueHandle_t xQueueDistance;

void pin_callback(uint gpio, uint32_t events) {
    int time = 0;
    if (events == 0x4) {
        end_time = to_us_since_boot(get_absolute_time());
    } else if (events == 0x8) {
        start_time = to_us_since_boot(get_absolute_time());
    }
    xQueueSendFromISR(xQueueTime, &time, 0);
}

void echo_task(void *p) {
    int distance;

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, 
                                                                &pin_callback);
    while(1) {
        bool measure = false;
        if (xQueueReceive(xQueueTime, &distance,  pdMS_TO_TICKS(100))) {
            distance = (end_time - start_time) * 0.01715;
            measure = true;
        } else {
            distance = -1000;
        }

        xQueueSend(xQueueDistance, &distance, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
}

void trigger_task(void *p) {
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    while (1) {
        if (xSemaphoreTake(xSemaphore_t, portMAX_DELAY) == pdTRUE) {
            gpio_put(TRIG_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(10));
            gpio_put(TRIG_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(60));
            xSemaphoreGive(xSemaphore_t);
        }
    }
}


void oled_task(void *p) {
    int distance;
    char distanceStr[32];
    const int maxWidth = 128;
    ssd1306_init();


    ssd1306_t disp;
    gfx_init(&disp, 128, 32);



    while (1) {
        if(xQueueReceive(xQueueDistance, &distance, portMAX_DELAY) == pdTRUE) {
            if (distance == -1000) {
                snprintf(distanceStr, sizeof(distanceStr), "Falha");
            } else if (distance >= 0){
                gfx_clear_buffer(&disp);
                snprintf(distanceStr, sizeof(distanceStr), "Distance: %d cm", distance);
                gfx_draw_string(&disp, 0, 0, 1, distanceStr);
                int barLength = (int)((distance / 100.0) * maxWidth);
                    if (barLength > maxWidth) {
                        barLength = maxWidth;
                    }
                gfx_draw_line(&disp, 0, 31, barLength, 31);
            }
            gfx_show(&disp);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

int main() {
    stdio_init_all();

    xSemaphore_t = xSemaphoreCreateBinary();
    xQueueTime = xQueueCreate(32, sizeof(int) );
    xQueueDistance = xQueueCreate(32, sizeof(int) );

    xTaskCreate(trigger_task, "Trigger Task", 256, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo Task", 256, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED Task", 256, NULL, 1, NULL);
    vTaskStartScheduler();

    while (true)
        ;
}

