/*******************************************************************************
* File Name:   peripheral_privacy.c
*
* Description: This is the source code for the Peripheral_Privacy Example
*              for ModusToolbox.
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

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "peripheral_privacy.h"
#include "cybsp.h"
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>
#include "stdio.h"
#include <inttypes.h>
#include "app_bt_bonding.h"
#include "app_bt_utils.h"
#include "cybt_platform_trace.h"
#include "wiced_memory.h"
#include "wiced_bt_stack.h"
#include "wiced_bt_dev.h"
#include "GeneratedSource/cycfg_gatt_db.h"
#include "GeneratedSource/cycfg_bt_settings.h"
#include "GeneratedSource/cycfg_gap.h"
#include "mtb_kvstore.h"
#include "retarget_io_init.h"

/*******************************************************************************
* Macro Definitions
*******************************************************************************/
#define GPIO_INTERRUPT_PRIORITY  (7U)
#define RESET_VAL                (0U)
#define OFFSET_VAL               (1U)
#define P_VAL_INDEX_0            (0U)
#define P_VAL_INDEX_1            (1U)
#define SHIFT_VAL                (8U)
#define DUTY_CYCLE_0             (0U)
#define DUTY_CYCLE_30            (600U)
#define DUTY_CYCLE_60            (1200U)
#define DUTY_CYCLE_100           (2000U)
#define TASK_DELAY_500MS         (500U)
#define DEBOUNCE_TIME_MS         (250U)
#define REQ_LEN                  (2U)
#define INTEGER_ASCII_DIFF       ('0')
/*******************************************************************************
* Variable Definitions
*******************************************************************************/
typedef void (*pfn_free_buffer_t)(uint8_t *);
static uint16_t connection_id = RESET_VAL;
static wiced_bt_device_address_t connected_bda;
static wiced_bt_ble_advert_mode_t *p_adv_mode = NULL;

/* If true we will go into bonding mode.
 * This will be set false if pre-existing bonding info is available */
static wiced_bool_t bond_mode = WICED_TRUE;

/* This is the index for the link keys,cccd and
 * privacy mode of the host we are currently bonded to */
static uint8_t bondindex = RESET_VAL;
bool pairing_mode;

/* enum for state machine*/
enum StateMachine
{
    IDLE_NO_DATA,
    IDLE_DATA,
    IDLE_PRIVACY_CHANGE,
    CONNECTED,
    BONDED
} state;

/* Variables for button debouncing */
volatile bool button_debouncing = false;
volatile uint32_t button_debounce_timestamp = 0;

/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: app_free_buffer
********************************************************************************
* Summary:
*  This function frees up the memory buffer
*
* Parameters:
*  uint8_t *p_data: Pointer to the buffer to be free
*
* Return:
*  None
*
*******************************************************************************/
static void app_free_buffer(uint8_t *p_buf)
{
    vPortFree(p_buf);
}

/*******************************************************************************
* Function Name: app_alloc_buffer
********************************************************************************
* Summary:
*  This function allocates a memory buffer
*
* Parameters:
*  int len: Length to allocate
*
* Return:
*  void* Pointer to the allocated buffer in memory
*
*******************************************************************************/
static void *app_alloc_buffer(int len)
{
    return pvPortMalloc(len);
}

/*******************************************************************************
* Function Name: ble_app_connect_handler
********************************************************************************
* Summary:
*  This function handles the connection and disconnection events. It also
*  stores the currently connected device information in hostinfo structure.
*
* Parameters:
*  p_conn_status  : contains information related to the connection/disconnection
*                  event.
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e
*                         in wiced_bt_gatt.h
*
*******************************************************************************/
wiced_bt_gatt_status_t
ble_app_connect_handler(wiced_bt_gatt_connection_status_t *p_conn_status)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_ERROR;

    if (NULL != p_conn_status)
    {
        if (p_conn_status->connected)
        {
           /* Device has connected. Add the old devices back to controller
            * address resolution list immediately after connection.
            */
            if (TRUE == pairing_mode)
            {
                app_bt_add_devices_to_address_resolution_db();
                pairing_mode = FALSE;
            }

            /* Device has connected */
            printf("Connected : BD Addr: ");
            print_bd_address(p_conn_status->bd_addr);
            printf("Connection ID '%d'\n", p_conn_status->conn_id);

            /* Handling the connection by updating connection ID */
            connection_id = p_conn_status->conn_id;
            state = CONNECTED;
            led_task_communicator(BTM_BLE_ADVERT_OFF);
        }
        else
        {
            /* Device has disconnected */
            printf("\nDisconnected : BD Addr: ");
            print_bd_address(p_conn_status->bd_addr);
            printf("Connection ID '%d', Reason '%s'\n", p_conn_status->conn_id,
                   get_bt_gatt_disconn_reason_name(p_conn_status->reason));
            led_task_communicator(BTM_BLE_ADVERT_OFF);

            /* Handling the disconnection */
            connection_id = RESET_VAL;

            /* Reset the CCCD value so that on a reconnect CCCD will be off */
            app_wicedbutton_mb1_client_char_config[0] = RESET_VAL;

            if (ZERO_BONDED_DEVICE < bond_info.slot_data[NUM_BONDED])
            {
                state = IDLE_DATA;
                print_device_selection_menu();
                printf("\r\nEnter slot number to start directed advertisement "
                        "for that device \r\n");
                printf("Enter 'E'   to start undirected Advertisement to add "
                        "new device to bonding list \r\n");
                bond_mode = WICED_FALSE;
            }
            else
            {
                state = IDLE_NO_DATA;
                bond_mode = WICED_TRUE;
                wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH,
                        BLE_ADDR_PUBLIC, NULL);
            }
        }

        status = WICED_BT_GATT_SUCCESS;
    }

    return status;
}

/*******************************************************************************
* Function Name: ble_app_set_value
********************************************************************************
* Summary:
*  This function handles writing to the attribute handle in the GATT
*  database using the data passed from the BT stack. The value to write is
*  stored in a buffer whose starting address is passed as one of the
*  function parameters
*
* Parameters:
*  uint16_t attr_handle : GATT attribute handle
*  uint8_t  p_val       : Pointer to LE GATT write request value
*  uint16_t len         : length of GATT write request
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in
*                                 wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
*******************************************************************************/
static wiced_bt_gatt_status_t ble_app_set_value(uint16_t attr_handle,
                                                uint8_t *p_val,
                                                uint16_t len)
{
    int i = RESET_VAL;
    wiced_bool_t isHandleInTable = WICED_FALSE;
    wiced_bool_t validLen = WICED_FALSE;
    wiced_bt_gatt_status_t res = WICED_BT_GATT_INVALID_HANDLE;
    cy_rslt_t rslt = CY_RSLT_SUCCESS;
    uint16_t cccd = RESET_VAL;

    /* Check for a matching handle entry */
    for (i = RESET_VAL; i < app_gatt_db_ext_attr_tbl_size; i++)
    {
        if (app_gatt_db_ext_attr_tbl[i].handle == attr_handle)
        {
            /* Detected a matching handle in external lookup table */
            isHandleInTable = WICED_TRUE;

            /* Verify that size constraints have been met */
            validLen = (app_gatt_db_ext_attr_tbl[i].max_len >= len);

            if (validLen)
            {
                /* Value fits within the supplied buffer; copy over the value */
                app_gatt_db_ext_attr_tbl[i].cur_len = len;
                memcpy(app_gatt_db_ext_attr_tbl[i].p_data, p_val, len);
                res = WICED_BT_GATT_SUCCESS;

                switch (attr_handle)
                {
                    case HDLD_WICEDBUTTON_MB1_CLIENT_CHAR_CONFIG:

                        if (REQ_LEN != len)
                        {
                            /* Value to write does not meet size constraints */
                            res = WICED_BT_GATT_INVALID_ATTR_LEN;
                            break;
                        }

                        /* Update CCCD Value in NVM */
                        cccd = (p_val[P_VAL_INDEX_0] | (p_val[P_VAL_INDEX_1] <<
                                SHIFT_VAL));
                        rslt = app_bt_update_cccd(cccd, bondindex);

                        if (CY_RSLT_SUCCESS != rslt)
                        {
                            printf("Failed to update CCCD Value in NVM! \r\n");
                        }
                        else
                        {
                            printf("CCCD value updated in NVM! \r\n");
                        }

                        break;

                    default:
                        printf("Write is not supported \r\n");
                }
            }
            else
            {
                /* Value to write does not meet size constraints */
                res = WICED_BT_GATT_INVALID_ATTR_LEN;
            }

            break;
        }
    }

    if (!isHandleInTable)
    {
        switch (attr_handle)
        {
            default:

                /* The write operation was not performed for the indicated handle */
                printf("Write Request to Invalid Handle: 0x%x\r\n", attr_handle);
                res = WICED_BT_GATT_WRITE_NOT_PERMIT;
                break;
        }
    }

    return res;
}

/*******************************************************************************
* Function Name: ble_app_write_handler
********************************************************************************
* Summary:
*  This function handles Write Requests received from the client device
*
* Parameters:
*  conn_id       Connection ID
*  opcode        BLE GATT request type opcode
*  p_write_req   Pointer to BLE GATT write request
*  len_req       length of data requested
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in
*                                   wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
*******************************************************************************/
static wiced_bt_gatt_status_t ble_app_write_handler(uint16_t conn_id,
          wiced_bt_gatt_opcode_t opcode, wiced_bt_gatt_write_req_t *p_write_req,
          uint16_t len_req, uint16_t *p_error_handle)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_INVALID_HANDLE;
    *p_error_handle = p_write_req->handle;

    /* Attempt to perform the Write Request */
    status = ble_app_set_value(p_write_req->handle,
                               p_write_req->p_val,
                               p_write_req->val_len);

    if (WICED_BT_GATT_SUCCESS != status)
    {
        printf("WARNING: GATT set attr status 0x%x\n", status);
    }

    return (status);
}

/*******************************************************************************
* Function Name: app_get_attribute
********************************************************************************
* Summary:
*  This function searches through the GATT DB to point to the attribute
*  corresponding to the given handle
*
* Parameters:
*  uint16_t handle: Handle to search for in the GATT DB
*
* Return:
*  gatt_db_lookup_table_t *: Pointer to the correct attribute in the GATT DB
*
*******************************************************************************/
static gatt_db_lookup_table_t *app_get_attribute(uint16_t handle)
{
   /* Search for the given handle in the GATT DB and return the pointer to the
    * correct attribute
    */
    uint8_t array_index = RESET_VAL;
    gatt_db_lookup_table_t *attribute = NULL;

    for (array_index = RESET_VAL; array_index < app_gatt_db_ext_attr_tbl_size;
            array_index++)
    {
        if (app_gatt_db_ext_attr_tbl[array_index].handle == handle)
        {
            attribute = &app_gatt_db_ext_attr_tbl[array_index];
               break;
        }
    }

    return attribute;
}


/*******************************************************************************
* Function Name: ble_app_read_handler
********************************************************************************
* Summary:
*  This function handles Read Requests received from the client device
*
* Parameters:
*  conn_id              : Connection ID
*  opcode               : LE GATT request type opcode
*  p_read_req           : Pointer to read request containing the handle
*                        to read
*  len_req              : length of data requested
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in
*                                 wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
*******************************************************************************/
static wiced_bt_gatt_status_t ble_app_read_handler(uint16_t conn_id,
            wiced_bt_gatt_opcode_t opcode, wiced_bt_gatt_read_t *p_read_req,
            uint16_t len_req, uint16_t *p_error_handle)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;
    gatt_db_lookup_table_t *puAttribute;
    int attr_len_to_copy;
    uint8_t *from;
    int to_send;
    *p_error_handle = p_read_req->handle;
    puAttribute = app_get_attribute(p_read_req->handle);

    if (NULL == puAttribute)
    {
        status = WICED_BT_GATT_INVALID_HANDLE;
    }
    else
    {
        attr_len_to_copy = puAttribute->cur_len;

        if (p_read_req->offset >= puAttribute->cur_len)
        {
            status = WICED_BT_GATT_INVALID_OFFSET;
        }
        else
        {
            to_send = MIN(len_req, attr_len_to_copy - p_read_req->offset);
            from = ((uint8_t *)puAttribute->p_data) + p_read_req->offset;
            status = wiced_bt_gatt_server_send_read_handle_rsp(conn_id,
                    opcode, to_send, from, NULL);
        }
    }

    return status;
}


/*******************************************************************************
* Function Name : ble_app_bt_gatt_req_read_by_type_handler
********************************************************************************
* Summary:
* Process read-by-type request from peer device
*
* Parameters:
* uint16_t conn_id                        : Connection ID
* wiced_bt_gatt_opcode_t opcode           : LE GATT request type opcode
* wiced_bt_gatt_read_by_type_t p_read_req : Pointer to read request
*          containing the handle to read
* uint16_t len_requested                  : Length of data requested
*
* Return:
* wiced_bt_gatt_status_t                  : LE GATT status
*
*******************************************************************************/
static wiced_bt_gatt_status_t ble_app_bt_gatt_req_read_by_type_handler
       (uint16_t conn_id, wiced_bt_gatt_opcode_t opcode,
        wiced_bt_gatt_read_by_type_t *p_read_req,
        uint16_t len_requested, uint16_t *p_error_handle)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;
    gatt_db_lookup_table_t *puAttribute;
    uint16_t last_handle = RESET_VAL;
    uint16_t attr_handle = p_read_req->s_handle;
    uint8_t *p_rsp = app_alloc_buffer(len_requested);
    uint8_t pair_len = RESET_VAL;
    int used = RESET_VAL;

    if (NULL == p_rsp)
    {
        printf("No memory, len_requested: %d!!\r\n", len_requested);
        status = WICED_BT_GATT_INSUF_RESOURCE;
    }
    else
    {
        /* Read by type returns all attributes of the specified type,
         * between the start and end handles */
        while (WICED_TRUE)
        {
            *p_error_handle = attr_handle;
            last_handle = attr_handle;
            attr_handle = wiced_bt_gatt_find_handle_by_type(attr_handle,
                      p_read_req->e_handle, &p_read_req->uuid);

            if (RESET_VAL == attr_handle)
            {
                break;
            }

            if (NULL == (puAttribute = app_get_attribute(attr_handle)))
            {
                printf("found type but no attribute for %d \r\n", last_handle);
                app_free_buffer(p_rsp);
                status = WICED_BT_GATT_INVALID_HANDLE;
                break;
            }

            int filled = wiced_bt_gatt_put_read_by_type_rsp_in_stream
                    (p_rsp + used, len_requested - used, &pair_len, attr_handle,
                     puAttribute->cur_len, puAttribute->p_data);

            if (RESET_VAL == filled)
            {
                break;
            }

            used += filled;

            /* Increment starting handle for next search to one past current */
            attr_handle++;
        }

        if (RESET_VAL == used)
        {
            printf("attr not found  start_handle: 0x%04x  end_handle: 0x%04x  "
                    "Type: 0x%04x\r\n", p_read_req->s_handle, p_read_req->e_handle,
                    p_read_req->uuid.uu.uuid16);
            app_free_buffer(p_rsp);
            status = WICED_BT_GATT_INVALID_HANDLE;
        }
        else
        {
            /* Send the response */
            status = wiced_bt_gatt_server_send_read_by_type_rsp(conn_id, opcode,
                    pair_len, used, p_rsp, (void *)app_free_buffer);
        }
    }

    return status;
}

/*******************************************************************************
* Function Name: ble_app_server_handler
********************************************************************************
* Summary:
* This function handles GATT server events from the BT stack.
*
* Parameters:
* p_attr_req             : Pointer to LE GATT connection status
*
* Return:
* wiced_bt_gatt_status_t: See possible status codes in
*                         wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
*******************************************************************************/
static wiced_bt_gatt_status_t
ble_app_server_handler(wiced_bt_gatt_attribute_request_t *p_data,
                       uint16_t *p_error_handle)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;
    wiced_bt_gatt_write_req_t *p_write_request = &p_data->data.write_req;

    switch (p_data->opcode)
    {
        case GATT_REQ_READ:
        case GATT_REQ_READ_BLOB:

            /* Attribute read request */
            status = ble_app_read_handler(p_data->conn_id, p_data->opcode,
                        &p_data->data.read_req, p_data->len_requested,
                        p_error_handle);
            break;

        case GATT_REQ_READ_BY_TYPE:
            status = ble_app_bt_gatt_req_read_by_type_handler(p_data->conn_id,
                         p_data->opcode, &p_data->data.read_by_type,
                         p_data->len_requested, p_error_handle);
            break;

        case GATT_REQ_WRITE:
        case GATT_CMD_WRITE:
        case GATT_CMD_SIGNED_WRITE:
            status = ble_app_write_handler(p_data->conn_id, p_data->opcode,
                      &p_data->data.write_req, p_data->len_requested,
                      p_error_handle);

            if ((GATT_REQ_WRITE == p_data->opcode) &&
                    (WICED_BT_GATT_SUCCESS == status))
            {
                wiced_bt_gatt_server_send_write_rsp(p_data->conn_id, p_data->opcode,
                                                    p_write_request->handle);
            }

            break;

        case GATT_REQ_MTU:

           /* Application calls wiced_bt_gatt_server_send_mtu_rsp() with desired
            * mtu.
            */
            status = wiced_bt_gatt_server_send_mtu_rsp(p_data->conn_id,
                     p_data->data.remote_mtu, cy_bt_cfg_settings.p_ble_cfg
                     ->ble_max_rx_pdu_size);
            break;

        case GATT_HANDLE_VALUE_NOTIF:
            printf("Client received our notification\r\n");
            status = WICED_BT_GATT_SUCCESS;
            break;

        default:
            printf("Unhandled Event opcode:%d\r\n", p_data->opcode);
            status = WICED_BT_GATT_ERROR;
            break;
    }

    return status;
}

/*******************************************************************************
* Function Name: ble_app_gatt_event_handler
********************************************************************************
* Summary:
* This function handles GATT events from the BT stack.
*
* Parameters:
* wiced_bt_gatt_evt_t event                : LE GATT event code of one
*                                            byte length
* wiced_bt_gatt_event_data_t *p_event_data : Pointer to LE GATT event
*                                            structures
*
* Return:
* wiced_bt_gatt_status_t                   : See possible status
*                                            codes in wiced_bt_gatt_status_e
*                                            in wiced_bt_gatt.h
*
*******************************************************************************/
static wiced_bt_gatt_status_t
ble_app_gatt_event_handler(wiced_bt_gatt_evt_t event,
                           wiced_bt_gatt_event_data_t *p_event_data)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;
    wiced_bt_gatt_attribute_request_t *p_attr_req =
            &p_event_data->attribute_request;
    uint16_t error_handle = RESET_VAL;

    /* Call the appropriate callback function based on the GATT event type,
     * and pass the relevant event parameters to the callback function */
    switch (event)
    {
        case GATT_CONNECTION_STATUS_EVT:
            status = ble_app_connect_handler(&p_event_data->connection_status);
            break;

        case GATT_ATTRIBUTE_REQUEST_EVT:
            status = ble_app_server_handler(p_attr_req, &error_handle);

            if (status != WICED_BT_GATT_SUCCESS)
            {
                wiced_bt_gatt_server_send_error_rsp(p_attr_req->conn_id,
                    p_attr_req->opcode, error_handle, status);
            }

            break;

        case GATT_GET_RESPONSE_BUFFER_EVT:

            /* GATT buffer request, typically sized to max of bearer mtu - 1 */
            p_event_data->buffer_request.buffer.p_app_rsp_buffer =
                app_alloc_buffer(p_event_data->buffer_request.len_requested);
            p_event_data->buffer_request.buffer.p_app_ctxt =
                    (void *)app_free_buffer;
            status = WICED_BT_GATT_SUCCESS;
            break;

        case GATT_APP_BUFFER_TRANSMITTED_EVT:
        {
           /* GATT buffer transmitted event,  check
            *  \ref wiced_bt_gatt_buffer_transmitted_t
            */
            pfn_free_buffer_t pfn_free = (pfn_free_buffer_t)p_event_data
                    ->buffer_xmitted.p_app_ctxt;

           /* If the buffer is dynamic, the context will point to a
            * function to free it.
            */
            if (pfn_free)
            {
                pfn_free(p_event_data->buffer_xmitted.p_app_data);
            }

            status = WICED_BT_GATT_SUCCESS;
        }
        break;

        default:
            status = WICED_BT_GATT_ERROR;
            break;
    }

    return status;
}

/*******************************************************************************
* Function Name: directed_adv_handler
********************************************************************************
* Summary:
* Directed advertisement Handler.
*
* Parameters:
* device_index : Index of the device stored in the device list to start directed
*                advertisement to.
*
* Return:
* None
*
*******************************************************************************/
void directed_adv_handler(uint8_t device_index)
{
    wiced_result_t result;
    wiced_bt_start_advertisements(BTM_BLE_ADVERT_OFF, 0, NULL);
    print_bd_address(bond_info.link_keys[device_index - OFFSET_VAL].bd_addr);
    printf("Enter 'E'   to start undirected Advertisement to add new "
            "device to bonding list\r\n");
    result = wiced_bt_start_advertisements(BTM_BLE_ADVERT_DIRECTED_HIGH,
            bond_info.link_keys[device_index - OFFSET_VAL].key_data.ble_addr_type,
            bond_info.link_keys[device_index - OFFSET_VAL].bd_addr);

    if (WICED_BT_SUCCESS != result)
    {
        printf("failed to start directed advertisement! \n");
    }
}

/*******************************************************************************
* Function Name: privacy_mode_handler
********************************************************************************
* Summary:
*  Handles request for change of privacy mode of bonded devices.
*
* Parameters:
*  device_index : Index of the device stored in the device list to change privacy
*  mode.
*
* Return:
*  None
*
*******************************************************************************/
void privacy_mode_handler(uint8_t device_index)
{
    cy_rslt_t rslt;
    bond_info.privacy_mode[device_index] ^= OFFSET_VAL;
    printf("Privacy Mode for device %d changed to (0 for Network, 1 for Device) "
            ":  %d \r\n", device_index, bond_info.privacy_mode[device_index]);
    rslt = mtb_kvstore_write(&kvstore_obj, "bond_data", (uint8_t *)&bond_info,
            sizeof(bond_info));

    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("Failed to update ");

    }

    wiced_bt_ble_set_privacy_mode(bond_info.link_keys[device_index].bd_addr,
                                  bond_info.link_keys[device_index].key_data.
                                  ble_addr_type,
                                  bond_info.privacy_mode[device_index]);
}

/*******************************************************************************
* Function Name: app_bt_management_callback
********************************************************************************
* Summary:
*  This is a Bluetooth stack event handler function to receive
*  management events from the LE stack and process as per the application.
*
* Parameters:
* wiced_bt_management_evt_t event             : BLE event code of
*                                               one byte length
* wiced_bt_management_evt_data_t *p_event_data: Pointer to BLE
*                                               management event structures
*
* Return:
* wiced_result_t  : Error code from WICED_RESULT_LIST or BT_RESULT_LIST
*
*******************************************************************************/
wiced_result_t app_bt_management_callback(wiced_bt_management_evt_t event,
        wiced_bt_management_evt_data_t *p_event_data)
{
    cy_rslt_t rslt;
    wiced_bt_dev_status_t status = WICED_BT_SUCCESS;
    wiced_bt_device_address_t bda = {RESET_VAL};
    wiced_bt_dev_ble_pairing_info_t *p_ble_info = NULL;

    switch (event)
    {
        case BTM_ENABLED_EVT:

            /* Bluetooth Controller and Host Stack Enabled */
            if (WICED_BT_SUCCESS == p_event_data->enabled.status)
            {
                /* Clear out the bond_info structure */
                memset(&bond_info, RESET_VAL, sizeof(bond_info));
                wiced_bt_dev_read_local_addr(bda);
                printf("Local Bluetooth Address: ");
                print_bd_address(bda);

                /* Perform application-specific initialization */
                ble_app_init();
            }
            else
            {
                printf("Bluetooth Disabled \n");
            }

            break;

        case BTM_DISABLED_EVT:

            /* Bluetooth Controller and Host Stack Disabled */
            printf("Bluetooth Disabled \r\n");
            break;

        case BTM_USER_CONFIRMATION_REQUEST_EVT:
            printf("\n********************\n");
            printf("* NUMERIC = %" PRIu32 " *\r", p_event_data->
                    user_confirmation_request.numeric_value);
            printf("\n********************\n");
            printf("Press 'y' if the numeric values match on both devices or "
                    "press 'n' if they do not.\n\n");
            memcpy(&(connected_bda), &(p_event_data->
                    user_confirmation_request.bd_addr),
                    sizeof(wiced_bt_device_address_t));
            break;

        case BTM_PASSKEY_NOTIFICATION_EVT:

            /* Print passkey to the screen so that the user can enter it. */
            printf("*************************************************************"
                    "*********\n");
            printf("Passkey Notification\n");
            printf("PassKey: %" PRIu32 " \n", p_event_data
                    ->user_passkey_notification.passkey);
            printf("*************************************************************"
                    "*********\n");

            /* confirming the passkey */
            wiced_bt_dev_confirm_req_reply(WICED_BT_SUCCESS, p_event_data
                    ->user_passkey_notification.bd_addr);
            break;

        case BTM_SECURITY_REQUEST_EVT:

            /* Security Request. Only grant if we are in bonding mode. */
            if (TRUE == bond_mode)
            {
                printf("Security Request Granted \n");
                wiced_bt_ble_security_grant(p_event_data->security_request.bd_addr,
                        WICED_SUCCESS);
            }
            else
            {
                printf("Security Request Denied - not in bonding mode \n");
            }

            break;

        case BTM_PAIRING_IO_CAPABILITIES_BLE_REQUEST_EVT:

            /* Request for Pairing IO Capabilities (BLE) */
            printf("BLE Pairing IO Capabilities Request\n");

            /* IO Capabilities on this Platform */
            p_event_data->pairing_io_capabilities_ble_request.local_io_cap =
                    BTM_IO_CAPABILITIES_DISPLAY_AND_YES_NO_INPUT;
            p_event_data->pairing_io_capabilities_ble_request.auth_req =
                    BTM_LE_AUTH_REQ_SC_MITM_BOND;
            p_event_data->pairing_io_capabilities_ble_request.init_keys =
                    BTM_LE_KEY_PENC | BTM_LE_KEY_PID;
            break;

        case BTM_BLE_CONNECTION_PARAM_UPDATE:
            printf("Connection parameter update status:%d, Connection Interval: %d, "
                    "Connection Latency: %d, Connection Timeout: %d\n",
                   p_event_data->ble_connection_param_update.status,
                   p_event_data->ble_connection_param_update.conn_interval,
                   p_event_data->ble_connection_param_update.conn_latency,
                   p_event_data->ble_connection_param_update.supervision_timeout);
            break;

        case BTM_PAIRING_COMPLETE_EVT:

            /* Pairing is Complete */
            p_ble_info = &p_event_data->pairing_complete.pairing_complete_info.ble;
            printf("Pairing Status %s \n", get_bt_smp_status_name(p_ble_info->reason));

            if (WICED_BT_SUCCESS == p_ble_info->reason) /* Bonding successful */
            {
                /* Update Num of bonded devices and next free slot in slot data*/
                rslt = app_bt_update_slot_data();

                /*Check if the data was updated successfully*/
                if (CY_RSLT_SUCCESS == rslt)
                {
                    printf("Slot Data saved to NVM \r\n");
                    printf("Successfully Bonded to: ");
                    print_bd_address(p_event_data->pairing_complete.bd_addr);
                }
                else
                {
                    printf("NVM Write Error \r\n");
                }

                /* remember that the device is now bonded, so disable bonding */
                bond_mode = FALSE;
                printf("Number of bonded devices: %d, Next free slot: %d, Number "
                        "of slots free: %d\n", bond_info.slot_data[NUM_BONDED],
                        bond_info.slot_data[NEXT_FREE_INDEX] + 1,
                        (BOND_INDEX_MAX - bond_info.slot_data[NUM_BONDED]));
            }
            else
            {
                printf("Bonding failed! \n");
            }

            break;

        case BTM_ENCRYPTION_STATUS_EVT:

            /* Encryption Status Change */
            printf("Encryption Status event for: ");
            print_bd_address(p_event_data->encryption_status.bd_addr);
            printf("Encryption Status event result: %d \n",
                    p_event_data->encryption_status.result);

           /*Check and retreive the index of the bond data of the device that
            * got connected. This call will return BOND_INDEX_MAX if the device
            * is not found */
            bondindex = app_bt_find_device_in_nvm(p_event_data
                    ->encryption_status.bd_addr);

            if (BOND_INDEX_MAX > bondindex)
            {
                app_bt_restore_bond_data();
                app_bt_restore_cccd();
                app_wicedbutton_mb1_client_char_config[0]
                                                       = peer_cccd_data[bondindex];

               /* Set CCCD value from the value that was previously saved in the
                * NVM.
                */
                printf("Bond info present in NVM for device: ");
                print_bd_address(p_event_data->encryption_status.bd_addr);
                state = BONDED;
            }
            else
            {
                printf("No Bond info present in NVM for device: ");
                print_bd_address(p_event_data->encryption_status.bd_addr);
                bondindex = RESET_VAL;
            }

            break;

        case BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT:

            /* save device keys to NVM */
            printf("Paired Device Key Update \r\n");
            rslt = app_bt_save_device_link_keys(&(p_event_data->
                    paired_device_link_keys_update));
            if (CY_RSLT_SUCCESS == rslt)
            {
                printf("Successfully Bonded to ");
                print_bd_address(p_event_data->
                        paired_device_link_keys_update.bd_addr);
            }
            else
            {
                printf("Failed to bond! \r\n");
            }

            break;

        case BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT:

            /* Paired Device Link Keys Request */
            printf("Paired Device Link keys Request Event for device ");
            print_bd_address((uint8_t *)(p_event_data->
                    paired_device_link_keys_request.bd_addr));
            status = WICED_BT_ERROR;

            /* This call will return BOND_INDEX_MAX if the device is not found*/
            bondindex = app_bt_find_device_in_nvm(p_event_data->
                    paired_device_link_keys_request.bd_addr);

            if (BOND_INDEX_MAX > bondindex)
            {
                /* Copy the keys to where the stack wants it */
                memcpy(&(p_event_data->paired_device_link_keys_request),
                        &(bond_info.link_keys[bondindex]),
                        sizeof(wiced_bt_device_link_keys_t));
                status = WICED_BT_SUCCESS;
            }
            else
            {
                printf("Device Link Keys not found in the database! \n");
                bondindex = RESET_VAL;
            }

            break;

        case BTM_LOCAL_IDENTITY_KEYS_UPDATE_EVT:

            /* Update of local privacy keys - save to NVM */
            printf("Local Identity Key Update\n");
            rslt = app_bt_save_local_identity_key(p_event_data->
                    local_identity_keys_update);

            if (CY_RSLT_SUCCESS != rslt)
            {
                status = WICED_BT_ERROR;
            }

            break;

        case BTM_LOCAL_IDENTITY_KEYS_REQUEST_EVT:

            /* Request for local privacy keys - read from NVM */
            app_kv_store_init();

            printf("Local Identity Key Request\r\n");

            /*Read Local Identity Resolution Keys*/
            rslt = app_bt_read_local_identity_keys();

            if (CY_RSLT_SUCCESS == rslt)
            {
                memcpy(&(p_event_data->local_identity_keys_request),
                       &(identity_keys), sizeof(wiced_bt_local_identity_keys_t));
                print_array(&identity_keys, sizeof(wiced_bt_local_identity_keys_t));
                status = WICED_BT_SUCCESS;
            }
            else
            {
                status = WICED_BT_ERROR;
            }

            break;

        case BTM_BLE_ADVERT_STATE_CHANGED_EVT:

            /* Advertisement State Changed */
            p_adv_mode = &p_event_data->ble_advert_state_changed;
            led_task_communicator(*p_adv_mode);
            printf("Advertisement State Change: %d\r\n", *p_adv_mode);
            break;

        default:
            printf("Unhandled Bluetooth Management Event: 0x%x %s\n",
                    event, get_btm_event_name(event));
            break;
    }

    return status;
}

/*******************************************************************************
* Function Name: ble_app_init
********************************************************************************
* Summary:
*  Initialize the ble advertisement and related functions.
*******************************************************************************/
void ble_app_init(void)
{

    /* These are needed for reading stored keys from Serial NVM */
    cy_rslt_t rslt;

    if (CY_RSLT_SUCCESS == app_bt_restore_bond_data())
    {
        printf("Keys found in NVRAM, add them to Addr Res DB\n");

        /* Load previous paired keys for address resolution */
        app_bt_add_devices_to_address_resolution_db();
    }

    /* Allow peer to pair */
    wiced_bt_set_pairable_mode(WICED_TRUE, WICED_FALSE);

    /* Set Advertisement Data */
    wiced_bt_ble_set_raw_advertisement_data(CY_BT_ADV_PACKET_DATA_SIZE,
                                            cy_bt_adv_packet_data);

    /* Register with stack to receive GATT callback */
    wiced_bt_gatt_register(ble_app_gatt_event_handler);

    /* Initialize GATT Database */
    wiced_bt_gatt_db_init(gatt_database, gatt_database_len, NULL);

    /* Read contents of Serial NVM */
    rslt = app_bt_restore_bond_data();

    if (CY_RSLT_SUCCESS == rslt)
    {
        printf("Bond data successfully restored from NVM!\r\n");
    }

    if (ZERO_BONDED_DEVICE == bond_info.slot_data[NUM_BONDED])
    {
        /* Allow new devices to bond */
        bond_mode = TRUE;
        printf("No bonded Device Found,Starting Undirected Advertisement "
                "\r\n\r\n");

        /* Start Undirected LE Advertisements on device startup. */
        wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH,
                BLE_ADDR_PUBLIC, NULL);

        /* Set current state to IDLE with no data*/
        state = IDLE_NO_DATA;
    }
    else
    {
        printf("Number of bonded devices: %d, Next free slot: %d, Number "
                "of slots free %d \r\n", bond_info.slot_data[NUM_BONDED],
               bond_info.slot_data[NEXT_FREE_INDEX] + OFFSET_VAL,
               (BOND_INDEX_MAX - bond_info.slot_data[NUM_BONDED]));
        printf("printing Bonded Device information: \r\n");

       /* New devices not allowed to bond, can be enabled by entering 'b' on
        * Terminal.
        */
        bond_mode = FALSE;
        print_bond_data();

        /* Change state to IDLE with bond data present*/
        state = IDLE_DATA;

        /* Add devices to address resolution database*/
        app_bt_add_devices_to_address_resolution_db();

        /*Start Advertisements*/
        if (ONE_BONDED_DEVICE == bond_info.slot_data[NUM_BONDED])
        {
            printf("\r\nOnly 1 Device Found,Starting directed Advertisement to: ");
            print_bd_address(bond_info.link_keys[0].bd_addr);
            print_bd_address(bond_info.link_keys[0].conn_addr);
            printf("Enter 'E'   to start undirected Advertisement to add "
                    "new device to bonding list\r\n");
            wiced_bt_start_advertisements(BTM_BLE_ADVERT_DIRECTED_LOW,
                    bond_info.link_keys[0].key_data.ble_addr_type,
                    bond_info.link_keys[0].bd_addr);
        }
        else
        {
            printf("\r\nSelect the bonded Devices Found in below list to "
                    "Start Directed Advertisement \r\n");
            print_device_selection_menu();
            printf("\r\nEnter slot number to start directed advertisement "
                    "for that device \r\n");
            printf("Enter 'E'   to start undirected Advertisement to add "
                    "new device to bonding list \r\n");
            printf("************************** NOTE *********************"
                    "******************************\r\n");
            printf("*ONCE THE SLOTS ARE FULL THE OLDEST DEVICE DATA WILL "
                    "BE OVERWRITTEN FOR NEW DEVICE*\r\n");
            printf("*****************************************************"
                    "******************************\r\n");
        }
    }
}

/*******************************************************************************
* Function Name: led_task_communicator
*******************************************************************************
* Summary:
*  This function handles the communication to the led task by pushing current
*  advertisement state to the LED queue which is processed by app_led_control
*
* Parameters:
*  CurrAdvState:Current Advertisement State
*
* Return:
*  None
*
*******************************************************************************/
void led_task_communicator(wiced_bt_ble_advert_mode_t CurrAdvState)
{

    /* Post the Current Advertisement State */
    if (pdPASS != xQueueSend(xLEDQueue, &CurrAdvState, (TickType_t)10))
    {
        printf("Failed to queue up Current Advertisement State!");
    }
}

/*******************************************************************************
* Function Name: app_led_control
********************************************************************************
* Summary:
*  This Function to toggle led state depending on the state of advertisement.
*         1. Advertisement ON (Undirected) : slow Blinking led(T = 1 sec)
*         2. Advertisement ON (Directed)   : fast Blinking led(T = 200 msec)
*         3. Advertisement OFF, Connected  : LED ON
*         4. Advertisement OFF, Timed out  : LED OFF
*
* Parameters:
*  void *pvParameters:
*  Not used
*
* Return:
*  None.
*
*******************************************************************************/
void app_led_control(void *pvParameters)
{
    cy_rslt_t rslt;
    wiced_bt_ble_advert_mode_t CurrAdvState;

    /* Initialize the TCPWM block */
    rslt= Cy_TCPWM_PWM_Init(CYBSP_PWM_LED_CTRL_HW,
            CYBSP_PWM_LED_CTRL_NUM, &CYBSP_PWM_LED_CTRL_config);

    /* PWM init failed. Stop program execution */
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("Advertisement LED PWM Initialization has failed! \n");
        handle_app_error();
    }

    /* Enable the TCPWM block */
    Cy_TCPWM_PWM_Enable(CYBSP_PWM_LED_CTRL_HW,
            CYBSP_PWM_LED_CTRL_NUM);

    while (1)
    {
        if (pdPASS == xQueueReceive(xLEDQueue, &(CurrAdvState), portMAX_DELAY))
        {
            switch (CurrAdvState)
            {
                case BTM_BLE_ADVERT_OFF:

                    if (RESET_VAL != connection_id)
                    {
                        Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_LED_CTRL_HW,
                                CYBSP_PWM_LED_CTRL_NUM,DUTY_CYCLE_0);
                    }
                    else
                    {
                        Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_LED_CTRL_HW,
                                CYBSP_PWM_LED_CTRL_NUM,DUTY_CYCLE_100);
                    }
                    break;

                case BTM_BLE_ADVERT_DIRECTED_HIGH:
                case BTM_BLE_ADVERT_DIRECTED_LOW:
                    Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_LED_CTRL_HW,
                            CYBSP_PWM_LED_CTRL_NUM,DUTY_CYCLE_30);
                    break;

                case BTM_BLE_ADVERT_UNDIRECTED_HIGH:
                case BTM_BLE_ADVERT_UNDIRECTED_LOW:
                    Cy_TCPWM_PWM_SetCompare0(CYBSP_PWM_LED_CTRL_HW,
                            CYBSP_PWM_LED_CTRL_NUM,DUTY_CYCLE_60);
                    break;

                default:
                    break;
            }
        }

        /* Start the PWM */
        Cy_TCPWM_TriggerStart_Single(CYBSP_PWM_LED_CTRL_HW,
                CYBSP_PWM_LED_CTRL_NUM);
    }
}

/*******************************************************************************
* Function Name: button_interrupt_handler
*******************************************************************************
* Summary:
*  This interrupt handler enables or disables GATT notifications upon button
*  press by notification.
*******************************************************************************/
static void button_interrupt_handler(void)
{
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;

    /* Check if the interrupt is from USER BUTTON 1 */
    if(Cy_GPIO_GetInterruptStatusMasked(CYBSP_USER_BTN_PORT,
            CYBSP_USER_BTN_PIN))
    {
        if (!button_debouncing)
        {
            /* Set the debouncing flag */
            button_debouncing = true;

            /* Record the current timestamp */
            button_debounce_timestamp = (uint32_t) (xTaskGetTickCount()
                * portTICK_PERIOD_MS);
        }

        if (button_debouncing && (((xTaskGetTickCount() * portTICK_PERIOD_MS)) -
                button_debounce_timestamp >= DEBOUNCE_TIME_MS *
                portTICK_PERIOD_MS))
        {
            button_debouncing = false;
            vTaskNotifyGiveFromISR(button_task_handle, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

            /* Clear the interrupt */
            Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_PIN);
            NVIC_ClearPendingIRQ(CYBSP_USER_BTN1_IRQ);
        }
    }

   /* CYBSP_USER_BTN1 (SW2) and CYBSP_USER_BTN2 (SW4) share the same port and
    * hence they share the same NVIC IRQ line. Since both the buttons are
    * configured for falling edge interrupt in the BSP, pressing any button
    * will trigger the execution of this ISR. Therefore, we must clear the
    * interrupt flag of the user button (CYBSP_USER_BTN2) to avoid issues in
    * case if user presses BTN2 by mistake.
    */
    Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN2_PORT, CYBSP_USER_BTN2_PIN);
    NVIC_ClearPendingIRQ(CYBSP_USER_BTN2_IRQ);
}

/*******************************************************************************
* Function Name: user_button_init
********************************************************************************
* Summary:
* This function configures the button for the interrupts.
*******************************************************************************/
void user_button_init(void)
{
    /* Interrupt config structure */
    cy_stc_sysint_t intrCfg =
    {
        .intrSrc = CYBSP_USER_BTN_IRQ,
        .intrPriority = GPIO_INTERRUPT_PRIORITY
    };

   /* CYBSP_USER_BTN1 (SW2) and CYBSP_USER_BTN2 (SW4) share the same port and
    * hence they share the same NVIC IRQ line. Since both are configured in the
    * BSP via the Device Configurator, the interrupt flags for both the buttons
    * are set right after they get initialized through the call to cybsp_init().
    * The flags must be cleared before initializing the interrupt, otherwise
    * the interrupt line will be constantly asserted.
    */
    Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN1_PORT, CYBSP_USER_BTN1_PIN);
    Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN2_PORT, CYBSP_USER_BTN2_PIN);
    NVIC_ClearPendingIRQ(CYBSP_USER_BTN1_IRQ);
    NVIC_ClearPendingIRQ(CYBSP_USER_BTN2_IRQ);

    /* Initialize the interrupt and register interrupt callback */
    cy_en_sysint_status_t btn_interrupt_init_status =
            Cy_SysInt_Init(&intrCfg, &button_interrupt_handler);

    if(CY_SYSINT_SUCCESS != btn_interrupt_init_status)
    {
        handle_app_error();
    }

    /* Enable the interrupt in the NVIC */
    NVIC_EnableIRQ(intrCfg.intrSrc);
}

/*******************************************************************************
* Function Name: button_task
********************************************************************************
* Summary:
*  This task starts Bluetooth LE advertisment on first button press and enables
*  or disables notifications from the server upon successive button presses.
*
* Parameters:
*  void *pvParameters:  Not used
*
* Return:
*  None
*
*******************************************************************************/
void button_task(void *pvParameters)
{
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* Increment the button value to register the button press */
        app_wicedbutton_mb1[RESET_VAL]++;

        /* If the connection is up and if the client wants notifications,
         * send updated button press value 
         */
        if (connection_id)
        {
            if (app_wicedbutton_mb1_client_char_config[RESET_VAL] &
                    GATT_CLIENT_CONFIG_NOTIFICATION)
            {
                wiced_bt_gatt_server_send_notification(connection_id,
                        HDLC_WICEDBUTTON_MB1_VALUE,
                        app_wicedbutton_mb1_len, app_wicedbutton_mb1, NULL);
                printf("Send Notification: sending Button value\r\n");
            }
            else
            {
                printf("Notifications are Disabled\r\n");
            }
        }
        else
        {
            printf("Connection is not Up \r\n");
        }
    }
}

/*******************************************************************************
* Function Name: uart_get_data()
********************************************************************************
* Summary:
*  This function reads a single byte from the UART interface,
*  blocking until data is available.
*
* Parameters:
*  value : uart data
*
* Return:
*  None
*
*******************************************************************************/
static void uart_get_data(uint8_t *value)
{
    uint32_t read_value = Cy_SCB_UART_Get(SCB2);

    while (CY_SCB_UART_RX_NO_DATA == read_value)
    {
        read_value = Cy_SCB_UART_Get(SCB2);
    }

    *value = (uint8_t)read_value;
}

/*******************************************************************************
* Function Name: uart_task
********************************************************************************
* Summary:
*  This function runs the UART task which processes the received commands via
*  Terminal.
*
* Parameters:
*  void *pvParameters:  Not used
*
* Return:
*  None
*
*******************************************************************************/
void uart_task(void *pvParameters)
{
    uint8_t readbyte = RESET_VAL;
    cy_rslt_t rslt = CY_RSLT_SUCCESS;
    uint8_t count = RESET_VAL;
    uint8_t device_index = RESET_VAL;

    if (setvbuf(stdin, NULL, _IONBF, RESET_VAL) != CY_RSLT_SUCCESS)
        {
            printf("Error: Unable to configure input buffer.\n");
        }

    for (;;)
    {
        uart_get_data(&readbyte);

        /* Extract Device Index for use wherever required*/
        device_index = readbyte - INTEGER_ASCII_DIFF;

        switch (readbyte)
        {
            case '1':

                if ((IDLE_DATA == state) && (ONE_BONDED_DEVICE <=
                    bond_info.slot_data[NUM_BONDED]))
                {
                    directed_adv_handler(device_index);
                }
                else if ((IDLE_PRIVACY_CHANGE == state) && (ONE_BONDED_DEVICE <=
                        bond_info.slot_data[NUM_BONDED]))
                {
                    privacy_mode_handler(device_index);

                    /*once privacy mode is changed go back to idle data state*/
                    state = IDLE_DATA;
                }
                else
                {
                    printf("Invalid Operation\r\n");
                }

                break;

            case '2':

                if ((IDLE_DATA == state) && (TWO_BONDED_DEVICES <=
                        bond_info.slot_data[NUM_BONDED]))
                {
                    directed_adv_handler(device_index);
                }
                else if ((IDLE_PRIVACY_CHANGE == state) && (TWO_BONDED_DEVICES
                        <= bond_info.slot_data[NUM_BONDED]))
                {
                    privacy_mode_handler(device_index);

                    /*once privacy mode is changed go back to idle data state*/
                    state = IDLE_DATA;
                }
                else
                {
                    printf("Invalid Operation\r\n");
                }

                break;

            case '3':

                if ((IDLE_DATA == state) && (THREE_BONDED_DEVICES <=
                        bond_info.slot_data[NUM_BONDED]))
                {
                    directed_adv_handler(device_index);
                }
                else if ((IDLE_PRIVACY_CHANGE == state) && (THREE_BONDED_DEVICES
                        <= bond_info.slot_data[NUM_BONDED]))
                {
                    privacy_mode_handler(device_index);

                    /*once privacy mode is changed go back to idle data state*/
                    state = IDLE_DATA;
                }
                else
                {
                    printf("Invalid Operation\r\n");
                }

                break;

            case '4':

                if ((IDLE_DATA == state) && (FOUR_BONDED_DEVICES ==
                        bond_info.slot_data[NUM_BONDED]))
                {
                    directed_adv_handler(device_index);
                }
                else if ((IDLE_PRIVACY_CHANGE == state) && (FOUR_BONDED_DEVICES
                        == bond_info.slot_data[NUM_BONDED]))
                {
                    privacy_mode_handler(device_index);
                    /*once privacy mode is changed go back to idle data state*/
                    state = IDLE_DATA;
                }
                else
                {
                    printf("Invalid Operation\r\n");
                }

                break;

            case 'd':
            case 'D':

                if (IDLE_DATA == state && BTM_BLE_ADVERT_DIRECTED_LOW !=
                        *p_adv_mode && BTM_BLE_ADVERT_DIRECTED_HIGH !=
                        *p_adv_mode)
                {
                    /* Put into bonding mode  */
                    bond_mode = TRUE;
                    rslt = app_bt_delete_bond_info();

                    if (CY_RSLT_SUCCESS == rslt)
                    {
                        printf("Erased NVM!\n");
                    }
                    else
                    {
                        printf("NVM Write Error!\n");
                    }

                    wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH,
                            BLE_ADDR_PUBLIC, NULL);

                    /* Change state to Idle and no data */
                    state = IDLE_NO_DATA;
                }
                else if (IDLE_NO_DATA == state)
                {
                    printf("No bond data present \r\n");
                }
                else
                {
                    printf("This option is not available when device is in "
                           "connected or bonded state or its doing directed "
                           "advertisement!!\r\n");
                }

                break;

            case 'e':
            case 'E':
                printf("************************** NOTE ***********************"
                        "****************************\r\n");
                printf("*ONCE THE SLOTS ARE FULL THE OLDEST DEVICE DATA WILL BE "
                        "OVERWRITTEN BY NEW DEVICE *\r\n");
                printf("********************************************************"
                        "***************************\r\n");

                if (!((CONNECTED == state) || (BONDED == state)))
                {
                    if (bond_mode == WICED_FALSE) /* Enter bond mode */
                    {
                        /* Check to see if we need to erase one of the existing
                         * devices */
                        if (bond_info.slot_data[NUM_BONDED] == BOND_INDEX_MAX)
                        {
                            printf("Bonding slots full removing the oldest device "
                                    "\r\n");

                            /* Remove oldest device from the bonded device list */
                            wiced_result_t result = app_bt_delete_device_info
                                    (bond_info.slot_data[NEXT_FREE_INDEX]);

                            if (WICED_BT_SUCCESS != result)
                            {
                                printf("error deleting device bond data!");
                            }

                            /* Reduce number of bonded devices by one */
                            bond_info.slot_data[NUM_BONDED]--;

                            /*Update bond information in NVM*/
                            rslt = app_bt_update_bond_data();

                            if (CY_RSLT_SUCCESS == rslt)
                            {
                                printf("Removed host: ");
                                print_bd_address((uint8_t *)&bond_info.link_keys
                                [bond_info.slot_data[NEXT_FREE_INDEX]].bd_addr);
                            }
                            else
                            {
                                printf("NVM Write Error, Cannot delete device!\n");
                            }
                        }

                        /* Put into bonding mode  */
                        bond_mode = WICED_TRUE;
                        printf("Bonding Mode Entered\r\n");
                        pairing_mode = TRUE;
                        wiced_result_t result =
                        wiced_bt_ble_address_resolution_list_clear_and_disable();

                        if (WICED_BT_SUCCESS == result)
                        {
                            printf("Address resolution list cleared successfully "
                                    "\n");
                        }
                        else
                        {
                            printf("Failed to clear address resolution list \n");
                        }

                        /* restart the advertisements in Bonding Mode */
                        wiced_bt_start_advertisements
                        (BTM_BLE_ADVERT_UNDIRECTED_HIGH, BLE_ADDR_PUBLIC, NULL);
                    }
                    else /* Exit bonding mode */
                    {
                        bond_mode = WICED_FALSE;
                        printf("Bonding Mode Exited\r\n");
                    }
                }
                else
                {
                    printf("This option is not available when device is in "
                           "connected or bonded state!!");
                }

                break;

            case 'h':
            case 'H':

                /* Print Display Menu */
                display_menu();
                break;

            case 'l':
            case 'L':
                printf("Number of bonded devices: %d, Next free slot: %d, "
                        "Number of free slot: %d \r\n",
                        bond_info.slot_data[NUM_BONDED],
                        bond_info.slot_data[NEXT_FREE_INDEX] + OFFSET_VAL,
                        (BOND_INDEX_MAX - bond_info.slot_data[NUM_BONDED]));

                for (count = RESET_VAL; count < bond_info.slot_data[NUM_BONDED];
                        count++)
                {
                    printf("Host %d: ", count + OFFSET_VAL);
                    print_bd_address(bond_info.link_keys[count].bd_addr);
                }

                break;

            case 'p':
            case 'P':

                /* If current state is bonded toggle current device privacy mode
                 * else print all devices and ask user for device to toggle
                 * Privacy mode */
                if (BONDED == state)
                {
                    privacy_mode_handler(bondindex);
                }
                else
                {
                    state = IDLE_PRIVACY_CHANGE;
                    printf("Select the bonded Devices Found in below list to "
                            "toggle current privacy mode \r\n\r\n");
                    print_device_selection_menu();
                    printf("\r\nEnter the slot number of the device to change "
                            "privacy mode: \r\n");
                }

                break;

            case 'y':
            case 'Y':

                /* Useful if using numeric comparison for pairing */
                wiced_bt_dev_confirm_req_reply(WICED_BT_SUCCESS, connected_bda);
                printf("Numeric Values are Matching!!\n");
                break;

            case 'n':
            case 'N':

                /* Useful if using numeric comparison for pairing */
                wiced_bt_dev_confirm_req_reply(WICED_BT_ERROR, connected_bda);
                printf("Numeric Values Don't Match\n");
                break;

            case 'r':
            case 'R':

                if (CONNECTED != state && BONDED != state &&
                        BTM_BLE_ADVERT_DIRECTED_LOW != *p_adv_mode &&
                        BTM_BLE_ADVERT_DIRECTED_HIGH != *p_adv_mode)
                {
                    /* Reset Kv-store library, this will clear the NVM */
                    rslt = mtb_kvstore_reset(&kvstore_obj);

                    if (CY_RSLT_SUCCESS == rslt)
                    {
                        printf("successfully reset kv-store library, "
                            "Please reset the device to generate new Keys!\r\n");
                    }
                    else
                    {
                        printf("failed to reset kv-store libray\r\n");
                    }

                    /* Clear bond_info structure */
                    memset(&bond_info, RESET_VAL, sizeof(bond_info));
                    wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH,
                            BLE_ADDR_PUBLIC, NULL);

                    /* Change state to Idle and no data */
                    state = IDLE_NO_DATA;

                    /* Put into bonding mode  */
                    bond_mode = TRUE;
                }
                break;

            default:
                printf("Invalid Input\r\n");

        }

        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_500MS));
    }
}

/*******************************************************************************
* Function Name: display_menu
********************************************************************************
* Summary:
*  Function to print the help menu.
*******************************************************************************/
void display_menu(void)
{
    printf("************************** MENU ***********************************"
            "********\r\n");
    printf("**1) Press 'L' to List for no of bonded devices and next empty slot"
            "      **\r\n");
    printf("**2) Press 'D' to Delete all the bond data present in NVM"
            "                **\r\n");
    printf("**3) Press 'E' to Enter the bonding mode and add devices to bond list"
            "    **\r\n");
    printf("**4) Enter 'slot number' to start directed advertisement for that "
            "device **\r\n");
    printf("**5) Press 'P' to change Privacy mode of bonded device       "
            "            **\r\n");
    printf("**6) Press 'H' to print this Help menu                     "
            "              **\r\n");
    printf("**7) Press 'R' to Reset kv-store (delete bond data and local IRK) "
            "       **\r\n");
    printf("******************************************************************"
            "*********\r\n");
}


/* END OF FILE [] */