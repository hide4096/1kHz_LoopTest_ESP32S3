#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/portmacro.h>
#include <freertos/queue.h>
#include "esp_timer.h"
#include "esp_system.h"
#include "driver/timer.h"
#include "sdkconfig.h"

#define LOOP_COUNT 100

int64_t when_interrupt[LOOP_COUNT] = {0};
QueueHandle_t notice;

inline void something(){
    for(int i = 0; i < 160*100; i++){
        __asm__ __volatile__ ("nop");
    }
}

void looptask_by_vTaskDelay(void *pvParameters){
    int count = 0;
    while(1){
        when_interrupt[count] = esp_timer_get_time();
        count++;
        something();
        if(count >= LOOP_COUNT){
            break;
        }
        vTaskDelay(1);
    }
    xQueueSend(notice, NULL, 0);
    vTaskDelete(NULL);
}

void looptask_by_vTaskDelayUntil(void *pvParameters){
    int count = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while(1){
        when_interrupt[count] = esp_timer_get_time();
        count++;
        something();
        if(count >= LOOP_COUNT){
            break;
        }
        vTaskDelayUntil(&xLastWakeTime, 1);
    }
    xQueueSend(notice, NULL, 0);
    vTaskDelete(NULL);
}

void IRAM_ATTR looptask_by_HardWareTimer(){
    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0);

    static int count = 0;
    when_interrupt[count] = esp_timer_get_time();
    count++;
    something();
    if(count >= 100){
        xQueueSendFromISR(notice, NULL, NULL);
    }
}

void HardwareTimer_init(){
    timer_config_t conf;
    conf.alarm_en = TIMER_ALARM_EN;
    conf.counter_en = TIMER_PAUSE;
    conf.clk_src = TIMER_SRC_CLK_APB;
    conf.intr_type = TIMER_INTR_LEVEL;
    conf.auto_reload = TIMER_AUTORELOAD_EN;
    conf.counter_dir = TIMER_COUNT_UP;
    conf.divider = 80;
    timer_init(TIMER_GROUP_0, TIMER_0, &conf);
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 1000);
    timer_isr_register(TIMER_GROUP_0, TIMER_0, looptask_by_HardWareTimer, NULL,0, NULL);
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
    timer_start(TIMER_GROUP_0, TIMER_0);
}

void app_main(void){
    notice = xQueueCreate(1,0);

    xTaskCreate(looptask_by_vTaskDelay, "vTaskDelay", 2048, NULL, 1, NULL);
    //portMAX_DELAYは永久待ちになる
    xQueueReceive(notice, NULL, portMAX_DELAY);
    printf("vTaskDelay\n");
    for(int i = 1; i < 100; i++){
        printf("%lld\n", when_interrupt[i] - when_interrupt[i-1]);
    }

    xTaskCreate(looptask_by_vTaskDelayUntil, "vTaskDelayUntil", 2048, NULL, 1, NULL);
    xQueueReceive(notice, NULL, portMAX_DELAY);
    printf("vTaskDelayUntil\n");
    for(int i = 1; i < 100; i++){
        printf("%lld\n", when_interrupt[i] - when_interrupt[i-1]);
    }

    HardwareTimer_init();    
    xQueueReceive(notice, NULL, portMAX_DELAY);
    timer_disable_intr(TIMER_GROUP_0, TIMER_0);
    printf("HardWareTimer\n");
    for(int i = 1; i < 100; i++){
        printf("%lld\n", when_interrupt[i] - when_interrupt[i-1]);
    }

    return;
}