/******************************************************************************
 *
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *     * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

/*******************************************************************************
 *
 *  Filename:      btif_lpm.cc
 *
 *  Description:   Implements interfaces with BTIF, BTA, SM and LPM Native
 *
 ******************************************************************************/

#include "btif_lpm.h"
#include "btif_lpm_interface.h"
#include "btif_storage.h"
#include "btif_common.h"
#include "osi/include/log.h"

#include <vector>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "btif_lpm"
#endif

/*******************************************************************************
 *  Structures and Macros
 ******************************************************************************/
#define BT_STATUS_OK 0x01

/*******************************************************************************
 *  Static variables
 ******************************************************************************/
static btif_lpm_cb_t btif_lpm_cb;

void btif_lpm_evt(tBTA_LPM_EVT event, tBTA_LPM* p_data);
/*******************************************************************************
* BTIF LPM Interface Functions
******************************************************************************/
/*******************************************************************************
*
* Function         send_prepare_offload_cmd_complete
*
* Description      Function to send prepare offload Command complete event to LPM SM
*
* Parameters       p_data - VSC command complete event structure
*
* Returns          None
*
******************************************************************************/
static void send_prepare_offload_cmd_complete(tBTM_VSC_CMPL* p_data) {
 LOG_INFO(LOG_TAG,"In send_prepare_offload_cmd_complete");
 uint8_t         *stream,  status;
 uint16_t        opcode, length;

 if (p_data && (stream = (uint8_t*)p_data->p_param_buf))
 {
   opcode = p_data->opcode;
   length = p_data->param_len;
   STREAM_TO_UINT8(status, stream);
   BTIF_TRACE_DEBUG("%s opcode = 0x%04X, length = %d, status = %d",
           __FUNCTION__, opcode, length, status);
   if (status == HCI_SUCCESS)
   {
     BTIF_TRACE_DEBUG("prepare offload cmd status success");
   }
   /* to be received from HIDL */
   btif_dispatch_lpm_event(BTIF_LPM_SM_CHECK_OFFLOAD_ACK_EVT,
           (void *)&status, DEFAULT_PARAM_LENGTH);
 }

/*******************************************************************************
 *
 * Function         send_enter_lpm_cmd_complete
 *
 * Description      Function to send enter lpm Command complete event to LPM SM
 *
 * Parameters       p_data - VSC command complete event structure
 *
 * Returns          None
 *
 ******************************************************************************/
static void send_enter_lpm_cmd_complete(tBTM_VSC_CMPL* p_data) {
  LOG_INFO(LOG_TAG,"In send_enter_lpm_cmd_complete");
  uint8_t         *stream,  status, subcmd;
  uint16_t        opcode, length;

  if (p_data && (stream = (uint8_t*)p_data->p_param_buf))
  {
    opcode = p_data->opcode;
    length = p_data->param_len;
    STREAM_TO_UINT8(status, stream);
    STREAM_TO_UINT8(subcmd, stream);
    BTIF_TRACE_DEBUG("%s opcode = 0x%04X, length = %d, status = %d, subcmd = %d",
            __FUNCTION__, opcode, length, status, subcmd);
    if (status == HCI_SUCCESS)
    {
      BTIF_TRACE_DEBUG("enter lpm cmd status success");
      status = BT_STATUS_OK;
    }
    btif_dispatch_lpm_event(BTIF_LPM_SM_LPM_ENTER_ACK_EVT,
                (void *)&status, DEFAULT_PARAM_LENGTH);
  }
}

/*******************************************************************************
 *
 * Function         send_exit_lpm_cmd_complete
 *
 * Description      Function to send exit lpm Command complete event to LPM SM
 *
 * Parameters       p_data - VSC command complete event structure
 *
 * Returns          None
 *
 ******************************************************************************/
static void send_exit_lpm_cmd_complete(tBTM_VSC_CMPL* p_data) {
  LOG_INFO(LOG_TAG,"In send_exit_lpm_cmd_complete");
  uint8_t         *stream,  status, subcmd;
  uint16_t        opcode, length;

  if (p_data && (stream = (uint8_t*)p_data->p_param_buf))
  {
    opcode = p_data->opcode;
    length = p_data->param_len;
    STREAM_TO_UINT8(status, stream);
    STREAM_TO_UINT8(subcmd, stream);
    BTIF_TRACE_DEBUG("%s opcode = 0x%04X, length = %d, status = %d, subcmd = %d",
            __FUNCTION__, opcode, length, status, subcmd);
    if (status == HCI_SUCCESS)
    {
      BTIF_TRACE_DEBUG("exit lpm cmd status success");
      status = BT_STATUS_OK;
    }
    /* take the backup of context info ?? */
    /* Send context info to stack */
    btif_dispatch_lpm_event(BTIF_LPM_SM_LPM_EXIT_EVT,
            (void *)&status, DEFAULT_PARAM_LENGTH);
  }
}

/*******************************************************************************
 *
 * Function         send_flow_on_cmd_complete
 *
 * Description      Function to send Flow On Command complete event to LPM SM
 *
 * Parameters       p_data - VSC command complete event structure
 *
 * Returns          None
 *
 ******************************************************************************/
static void send_flow_on_cmd_complete(tBTM_VSC_CMPL* p_data) {
  LOG_INFO(LOG_TAG,"In send_flow_on_cmd_complete");
  uint8_t         *stream,  status, subcmd;
  uint16_t        opcode, length;

  if (p_data && (stream = (uint8_t*)p_data->p_param_buf))
  {
    opcode = p_data->opcode;
    length = p_data->param_len;
    STREAM_TO_UINT8(status, stream);
    STREAM_TO_UINT8(subcmd, stream);
    BTIF_TRACE_DEBUG("%s opcode = 0x%04X, length = %d, status = %d, subcmd = %d",
            __FUNCTION__, opcode, length, status, subcmd);
    if (status == HCI_SUCCESS)
    {
      BTIF_TRACE_DEBUG("flow on cmd status success");
      status = BT_STATUS_OK;
    }
    btif_dispatch_lpm_event(BTIF_LPM_SM_FLOW_ON_EVT,
                (void *)&status, DEFAULT_PARAM_LENGTH);
  }
}

/*******************************************************************************
 *
 * Function         send_flow_off_cmd_complete
 *
 * Description      Function to send Flow Off Command complete event to LPM SM
 *
 * Parameters       p_data - VSC command complete event structure
 *
 * Returns          None
 *
 ******************************************************************************/
static void send_flow_off_cmd_complete(tBTM_VSC_CMPL* p_data) {
  LOG_INFO(LOG_TAG,"In send_flow_off_cmd_complete");
  uint8_t         *stream,  status, subcmd;
  uint16_t        opcode, length;

  if (p_data && (stream = (uint8_t*)p_data->p_param_buf))
  {
    opcode = p_data->opcode;
    length = p_data->param_len;
    STREAM_TO_UINT8(status, stream);
    STREAM_TO_UINT8(subcmd, stream);
    BTIF_TRACE_DEBUG("%s opcode = 0x%04X, length = %d, status = %d, subcmd = %d",
            __FUNCTION__, opcode, length, status, subcmd);
    if (status == HCI_SUCCESS)
    {
      BTIF_TRACE_DEBUG("flow off cmd status success");
      status = BT_STATUS_OK;
    }
    btif_dispatch_lpm_event(BTIF_LPM_SM_FLOW_OFF_EVT,
                (void *)&status, DEFAULT_PARAM_LENGTH);
  }
}

/*******************************************************************************
 *
 * Function         btif_lpm_init
 *
 * Description      Initializes LPM Profile
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_init() {
  BTIF_TRACE_DEBUG(" %s ", __FUNCTION__);

  /* Initialize LPM State Machine */
  /* Send LPM Init to LPM State Machine */

  BTA_EnableLPM(btif_lpm_evt);
}

/*******************************************************************************
 *
 * Function         btif_lpm_deinit
 *
 * Description      De-initializes LPM Profile
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_deinit() {
  BTIF_TRACE_DEBUG(" %s ", __FUNCTION__);

  /* Deinitialize LPM state machine */

  BTA_DisableLPM();
}

/*******************************************************************************
 *
 * Function         btif_lpm_init_done
 *
 * Description      Event function after LPM Profile initialialized
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_init_done() {
  BTIF_TRACE_DEBUG(" %s ", __FUNCTION__);

  bt_bdname_t bdname;
  RawAddress bdaddr;
  bt_property_t prop_name;
  bt_property_t prop_addr;
  btif_lpm_cb.privacy_enabled = false;
#if (BLE_LOCAL_PRIVACY_ENABLED == TRUE)
  btif_lpm_cb.privacy_enabled = true;
#endif
  /* Fetch adapter properties */
  BTIF_STORAGE_FILL_PROPERTY(&prop_addr, BT_PROPERTY_BDADDR,
                             sizeof(bdaddr), &bdaddr);
  if(btif_storage_get_adapter_property(&prop_addr) == BT_STATUS_SUCCESS) {
    btif_lpm_cb.device_addr = bdaddr.ToString();
  /* Call LPM Native interface function */
  }

  BTIF_STORAGE_FILL_PROPERTY(&prop_name, BT_PROPERTY_BDNAME,
                            sizeof(bt_bdname_t), &bdname);

  if (btif_storage_get_adapter_property(&prop_name) == BT_STATUS_SUCCESS) {
    strlcpy(btif_lpm_cb.device_name, (char*) bdname.name, BD_NAME_LEN + 1);
  /* Call LPM Native interface function */
  }
}

/*******************************************************************************
 *
 * Function         btif_lpm_send_prepare_offload_cmd
 *
 * Description      Function to send Prepare Offload Command to stack
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_send_prepare_offload_cmd(void) {
  BTIF_TRACE_EVENT("%s", __FUNCTION__);
  BTM_VendorSpecificCommand(HCI_BLE_LPM_PREPARE_OFFLOAD_OPCODE, 0, NULL,
      send_prepare_offload_cmd_complete);
}

/*******************************************************************************
 *
 * Function         btif_lpm_send_enter_lpm_cmd
 *
 * Description      Function to send Enter LPM Command to stack
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_send_enter_lpm_cmd(void) {
  BTIF_TRACE_EVENT("%s", __FUNCTION__);
  uint8_t* msg = (uint8_t *)osi_malloc(DEFAULT_PARAM_LENGTH);
  uint8_t *pp = (uint8_t *)msg;
  *pp++ = HCI_BLE_LPM_ENTER_SUB_OPCODE;
  BTM_VendorSpecificCommand(HCI_BLE_LPM_VSC_OPCODE, DEFAULT_PARAM_LENGTH, msg,
    send_enter_lpm_cmd_complete);
  osi_free(msg);
}

/*******************************************************************************
 *
 * Function         btif_lpm_send_exit_lpm_cmd
 *
 * Description      Function to send Exit LPM Command to stack
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_send_exit_lpm_cmd(void) {
  BTIF_TRACE_EVENT("%s", __FUNCTION__);
  uint8_t* msg = (uint8_t *)osi_malloc(DEFAULT_PARAM_LENGTH);
  uint8_t *pp = (uint8_t *)msg;
  *pp++ = HCI_BLE_LPM_EXIT_SUB_OPCODE;
  BTM_VendorSpecificCommand(HCI_BLE_LPM_VSC_OPCODE, DEFAULT_PARAM_LENGTH, msg,
    send_exit_lpm_cmd_complete);
  osi_free(msg);
}

/*******************************************************************************
 *
 * Function         btif_lpm_send_flow_on_cmd
 *
 * Description      Function to send Flow On Command to stack
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_send_flow_on_cmd(void) {
  BTIF_TRACE_EVENT("%s", __FUNCTION__);
  uint8_t* msg = (uint8_t *)osi_malloc(DEFAULT_PARAM_LENGTH);
  uint8_t *pp = (uint8_t *)msg;
  *pp++ = HCI_BLE_FLOW_ON_SUB_OPCODE;
  BTM_VendorSpecificCommand(HCI_BLE_LPM_VSC_OPCODE, DEFAULT_PARAM_LENGTH, msg,
    send_flow_on_cmd_complete);
  osi_free(msg);
}

/*******************************************************************************
 *
 * Function         btif_lpm_send_flow_off_cmd
 *
 * Description      Function to send Flow Off Command to stack
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_send_flow_off_cmd(void) {
  BTIF_TRACE_EVENT("%s", __FUNCTION__);
  uint8_t* msg = (uint8_t *)osi_malloc(DEFAULT_PARAM_LENGTH);
  uint8_t *pp = (uint8_t *)msg;
  *pp++ = HCI_BLE_FLOW_OFF_SUB_OPCODE;
  BTM_VendorSpecificCommand(HCI_BLE_LPM_VSC_OPCODE, DEFAULT_PARAM_LENGTH, msg,
    send_flow_off_cmd_complete);
  osi_free(msg);
}

/*******************************************************************************
 *
 * Function         btif_lpm_check_timer_activity
 *
 * Description      Function to check timer actiivty status and send events to
 *                  LPM SM accordingly
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_check_timer_activity(void) {
  bool is_timer_active = false;

  // Send Timer Active or Timer Inactive Event to SM
  if(is_timer_active) {
      btif_dispatch_lpm_event(BTIF_LPM_SM_TIMER_ACTIVE_EVT,
                  NULL, 0);
  } else {
      btif_dispatch_lpm_event(BTIF_LPM_SM_TIMER_INACTIVE_EVT,
                  NULL, 0);
  }

/*******************************************************************************
 *
 * Function         btif_lpm_process_profile_update
 *
 * Description      Handles BTA Profile update
 *
 * Parameters       profile_id - BTA sys id
 *                  profile_evt - Profile update
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_process_profile_update(uint8_t profile_id,
                                  tBTA_LPM_PROFILE_UPDATE_EVT profile_evt) {
  /* Post Profile update to state machine */
}

/*******************************************************************************
 *
 * Function         btif_lpm_upstreams_evt
 *
 * Description      Internal function which handles BTA LPM events
 *
 * Parameters       event - BTA LPM event
 *                  p_param - Data associated with the event
 *
 * Returns          None
 *
 ******************************************************************************/
static void btif_lpm_upstreams_evt(uint16_t event, char* p_param) {
  tBTA_LPM* p_data = (tBTA_LPM*)p_param;
  BTIF_TRACE_DEBUG(" %s: event : %d ", __FUNCTION__, event);
  switch(event) {
    case LPM_PROFILE_UPDATE_EVT: {
      btif_lpm_process_profile_update(p_data->profile_update.profile_id,
                                          p_data->profile_update.profile_evt);
      break;
    }
    default:
      break;
  }
}

/*******************************************************************************
 *
 * Function         btif_lpm_evt
 *
 * Description      Callback function
 *
 * Parameters       event - BTA LPM event
 *                  p_data - Data associated with the event
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_evt(tBTA_LPM_EVT event, tBTA_LPM* p_data) {
  /* switch context to btif task context */
  bt_status_t status = btif_transfer_context(
      btif_lpm_upstreams_evt, (uint16_t)event, (char*)p_data,
      sizeof(tBTA_LPM), NULL);

  ASSERTC(status == BT_STATUS_SUCCESS, "context transfer failed", status);
}
