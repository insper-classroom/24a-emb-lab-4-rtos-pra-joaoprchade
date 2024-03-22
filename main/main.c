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

typedef struct {
    int start_time;
    int end_time;
} Time;

SemaphoreHandle_t xSemaphore_t;
QueueHandle_t xQueueTime;
QueueHandle_t xQueueDistance;


void pin_callback(uint gpio, uint32_t events) {
    Time time;

    static int time_init;
    if (events == 0x8) {
        time_init = to_us_since_boot(get_absolute_time());
    } else if (events == 0x4) {
        time.start_time = time_init;
        time.end_time = to_us_since_boot(get_absolute_time());
        xQueueSendFromISR(xQueueTime, &time, NULL);
    }
}

void echo_task(void *p) {
    Time time;

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, 
                                                                &pin_callback);
    int distance;
    while(1) {
        if (xQueueReceive(xQueueTime, &time, pdMS_TO_TICKS(1000)) == pdTRUE) {
            distance = (time.start_time - time.end_time) / 58;
            distance = abs(distance);
            xQueueSend(xQueueDistance, &distance, 0);
        } else {
            distance = -1000;
            xQueueSend(xQueueDistance, &distance, 0);
        }

        }
        vTaskDelay(pdMS_TO_TICKS(1000));
}

void trigger_task(void *p) {
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);

    while (1) {
        gpio_put(TRIG_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_put(TRIG_PIN, 0);
        xSemaphoreGive(xSemaphore_t);
        vTaskDelay(pdMS_TO_TICKS(990));
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
        if (xSemaphoreTake(xSemaphore_t, pdMS_TO_TICKS(100)) == pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(150));
            if(xQueueReceive(xQueueDistance, &distance, pdMS_TO_TICKS(1000)) == pdTRUE) {
                if (distance == -1000) {
                    gfx_clear_buffer(&disp);
                    snprintf(distanceStr, sizeof(distanceStr), "Falha");
                    gfx_draw_string(&disp, 0, 0, 1, distanceStr);
                    gfx_show(&disp);
                    vTaskDelay(pdMS_TO_TICKS(150));
                } else {
                    gfx_clear_buffer(&disp);
                    snprintf(distanceStr, sizeof(distanceStr), "Distance: %d cm", distance);
                    gfx_draw_string(&disp, 0, 0, 1, distanceStr);
                    int barLength = (int)((distance / 100.00) * maxWidth);
                        if (barLength > maxWidth) {
                            barLength = maxWidth;
                        }
                    gfx_draw_line(&disp, 0, 31, barLength, 31);
                    gfx_show(&disp);
                    vTaskDelay(pdMS_TO_TICKS(150));
                }
            }
        }
    }
}

int main() {
    stdio_init_all();

    xSemaphore_t = xSemaphoreCreateBinary();

    xQueueTime = xQueueCreate(1, sizeof(Time) );
    xQueueDistance = xQueueCreate(1, sizeof(int));

    xTaskCreate(trigger_task, "Trigger Task", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo Task", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED Task", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}

