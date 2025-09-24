/******************************************************************************
* File Name: peripheral_privacy.h
*
* Description: This file is the public interface of peripheral_privacy.h
*
* Related Document: See README.md
*
********************************************************************************
* Copyright 2023-2025, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#ifndef __PERIPHERAL_PRIVACY_H_
#define __PERIPHERAL_PRIVACY_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "wiced_bt_dev.h"
#include <FreeRTOS.h>
#include <task.h>
#include "timers.h"
#include "wiced_bt_ble.h"
#include "queue.h"

/*******************************************************************************
* Macro Definitions
*******************************************************************************/

/* LED Task priority */
#define LED_TASK_PRIORITY          (configMAX_PRIORITIES - 4U)
#define LED_TASK_STACK_SIZE        (configMINIMAL_STACK_SIZE * 2U)

/*UART Interrupt priority*/
#define INT_PRIORITY                 (3U)
#define UART_TASK_PRIORITY          (configMAX_PRIORITIES - 4U)
#define UART_TASK_STACK_SIZE        (configMINIMAL_STACK_SIZE * 3U)

/* Macros for button interrupt and button task */
/* Interrupt priority for the GPIO connected to the user button */
#define BUTTON_INTERRUPT_PRIORITY          (3U)
#define BUTTON_TASK_PRIORITY               (configMAX_PRIORITIES - 4U)
#define BUTTON_TASK_STACK_SIZE             (configMINIMAL_STACK_SIZE * 3U)

/* Queue size for LED and UART tasks*/
#define QUEUE_SIZE                   (1U)
#define UART_INPUT_TIMEOUT_MS        (100U)

/*******************************************************************************
 * Variables
 ******************************************************************************/
extern TaskHandle_t button_task_handle;
extern QueueHandle_t xLEDQueue;
extern QueueHandle_t xUARTQueue;

/*******************************************************************************
 * Function Prototypes
 *******************************************************************************/
/*App feature and utility functions*/
void display_menu(void);
void ble_app_init(void);
void app_kv_store_init(void);

/*Button interrupt configuration and button task*/
void user_button_init(void);
void button_task(void *pvParameters);

/*LED state handlers*/
void led_task_communicator(wiced_bt_ble_advert_mode_t CurrAdvState);
void app_led_control(void *pvParameters);

/* Uart task for handling UART inputs */
void uart_task(void *pvParameters);

/* Callback function for Bluetooth stack management events */
wiced_bt_dev_status_t app_bt_management_callback(wiced_bt_management_evt_t event,
                      wiced_bt_management_evt_data_t *p_event_data);

#ifdef __cplusplus
}

#endif /* __cplusplus */

#endif /*__PERIPHERAL_PRIVACY_H_*/


/* [] END OF FILE */