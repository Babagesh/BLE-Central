/***************************************************************************//**
 * @file
 * @brief FreeRTOS helper functions for the application task.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "sl_main_init.h"
#include "app_assert.h"
#include "app.h"

#define APP_TASK_NAME          "app_task"
#define APP_TASK_STACK_SIZE    512u
#define APP_TASK_PRIO          24u
#define APP_MUTEX_WAIT         100 // Timeout to wait for mutex in ticks

// Application task.
static void app_task(void *p_arg);
extern SemaphoreHandle_t ble_sync_sem;
extern uint8_t target_connection;
extern uint16_t target_characteristic;
extern uint8_t rtos_operation_type;

// Task handle
static TaskHandle_t      app_task_handle  = NULL;
// Semaphore handle
static SemaphoreHandle_t app_semaphore_handle = NULL;
// Mutex handle
static SemaphoreHandle_t app_mutex_handle = NULL;

// Application Runtime Init.
void app_init_bt(void)
{
  BaseType_t ret;
  // Create the task for sl_app_process_action
  ret = xTaskCreate(app_task,
                    APP_TASK_NAME,
                    APP_TASK_STACK_SIZE,
                    NULL,
                    APP_TASK_PRIO,
                    &app_task_handle);
  app_assert(ret == pdPASS, "Application task creation failed.");
  // Create the semaphore
  app_semaphore_handle = xSemaphoreCreateCounting(UINT16_MAX, 0);
  app_assert(app_semaphore_handle != NULL, "Semaphore creation failed.");
  // Create the mutex
  app_mutex_handle = xSemaphoreCreateRecursiveMutex();
  app_assert(app_mutex_handle != NULL, "Mutex creation failed.");
}

/******************************************************************************
 * Application task.
 *****************************************************************************/
static void app_task(void *p_arg)
{
  xSemaphoreTake(ble_sync_sem, portMAX_DELAY);

      for (uint8_t i = 0; i < 5; i++) {

          // --- PERFORM WRITE ---
          uint8_t write_data[] = {0x33, i * 0x11};
          rtos_operation_type = 2; // Tell app.c we are writing

          sl_status_t sc = sl_bt_gatt_write_characteristic_value(target_connection,
                                                                 target_characteristic,
                                                                 sizeof(write_data),
                                                                 write_data);
          app_log_info("Initiating Write Request ... Code: 0x%lX\r\n", (unsigned long)sc);

          xSemaphoreTake(ble_sync_sem, portMAX_DELAY);

          rtos_operation_type = 1; // Tell app.c we are reading

          sc = sl_bt_gatt_read_characteristic_value(target_connection,
                                                    target_characteristic);
          app_log_info("Initiating Read Request ... Code: 0x%lX\r\n", (unsigned long)sc);

          xSemaphoreTake(ble_sync_sem, portMAX_DELAY);
      }

      vTaskSuspend(NULL);
}

// Proceed with execution.
void app_proceed(void)
{
  if (xPortIsInsideInterrupt()) {
    // Interrupt context
    BaseType_t woken = pdFALSE;
    (void)xSemaphoreGiveFromISR(app_semaphore_handle, &woken);
    portYIELD_FROM_ISR(woken);
  } else {
    // Non-interrupt context
    (void)xSemaphoreGive(app_semaphore_handle);
  }
}

// Check if it is required to process with execution.
bool app_is_process_required(void)
{
  BaseType_t ret = xSemaphoreTake(app_semaphore_handle, portMAX_DELAY);
  return (ret == pdTRUE);
}

// Acquire access to protected variables
bool app_mutex_acquire(void)
{
  BaseType_t response;
  response = xSemaphoreTakeRecursive(app_mutex_handle, (TickType_t)APP_MUTEX_WAIT);
  return response == pdTRUE;
}

// Finish access to protected variables
void app_mutex_release(void)
{
  (void)xSemaphoreGiveRecursive(app_mutex_handle);
}
