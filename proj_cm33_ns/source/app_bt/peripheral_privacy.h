/******************************************************************************
* File Name: peripheral_privacy.h
*
* Description: This file is the public interface of peripheral_privacy.h
*
* Related Document: See README.md
*
********************************************************************************
* (c) 2023-2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
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