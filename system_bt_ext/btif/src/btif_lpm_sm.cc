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

#include "btif_lpm.h"
#include "btif_lpm_interface.h"
#include "btif_common.h"

using namespace vendor::qti::hardware::bluetooth_offload::V1_0;
/*******************************************************************************
 *  Structures and Macros
 ******************************************************************************/
btif_sm_handle_t lpm_sm_handle;
/*******************************************************************************
 *  Static variables
 ******************************************************************************/
/* Flag to indicate flow on/off status */
static bool btif_lpm_flow_off_flag = false;
static bool btif_lpm_is_bt_active_flag = false;
static bool btif_lpm_is_bt_sco_active_flag = false;

/*******************************************************************************
 * BTIF LPM SM Function declarations
 *******************************************************************************/

static bool btif_lpm_state_stack_idle_handler(btif_sm_event_t event,
    void* data, int idx);
static bool btif_lpm_state_stack_active_handler(btif_sm_event_t event,
    void* data, int idx);
static bool btif_lpm_state_stack_active_pending_handler(btif_sm_event_t event,
    void* data, int idx);
static bool btif_lpm_state_stack_offload_pending_handler(btif_sm_event_t event,
    void* data, int idx);
static bool btif_lpm_state_stack_offloaded_handler(btif_sm_event_t event,
    void* data, int idx);

static const btif_sm_handler_t btif_lpm_sm_state_handlers[] = {
  btif_lpm_state_stack_idle_handler,
  btif_lpm_state_stack_active_handler,
  btif_lpm_state_stack_active_pending_handler,
  btif_lpm_state_stack_offload_pending_handler,
  btif_lpm_state_stack_offloaded_handler
};
/*******************************************************************************
 *
 * Function         dump_btif_lpm_sm_event_name
 *
 * Description      Converts state machine events into string of event name
 *
 * Parameters       event - LPM SM Event
 *
 * Returns          String of event name
 *
 ******************************************************************************/
const char* dump_btif_lpm_sm_event_name(btif_lpm_sm_event_t event) {
  switch ((int)event) {
    CASE_RETURN_STR(BTIF_LPM_SM_INIT_LPM_PROFILE)
    CASE_RETURN_STR(BTIF_LPM_SM_REINIT_LPM_NATIVE)
    CASE_RETURN_STR(BTIF_LPM_SM_SEND_LPM_ENTER_CMD)
    CASE_RETURN_STR(BTIF_LPM_SM_SEND_LPM_EXIT_CMD)
    CASE_RETURN_STR(BTIF_LPM_SM_CHECK_OFFLOAD_CMD)
    CASE_RETURN_STR(BTIF_LPM_SM_SEND_FLOW_ON_CMD)
    CASE_RETURN_STR(BTIF_LPM_SM_SEND_FLOW_OFF_CMD)
    CASE_RETURN_STR(BTIF_LPM_SM_LPM_ENTER_ACK_EVT)
    CASE_RETURN_STR(BTIF_LPM_SM_CHECK_OFFLOAD_ACK_EVT)
    CASE_RETURN_STR(BTIF_LPM_SM_LPM_EXIT_EVT)
    CASE_RETURN_STR(BTIF_LPM_SM_FLOW_ON_EVT)
    CASE_RETURN_STR(BTIF_LPM_SM_FLOW_OFF_EVT)
    CASE_RETURN_STR(BTIF_LPM_SM_TIMER_ACTIVE_EVT)
    CASE_RETURN_STR(BTIF_LPM_SM_TIMER_INACTIVE_EVT)
    CASE_RETURN_STR(BTIF_LPM_SM_PROFILE_UPDATE_EVT)
    CASE_RETURN_STR(BTIF_LPM_SM_NOTIFY_NACK_MSG)
    CASE_RETURN_STR(BTIF_SM_ENTER_EVT)
    CASE_RETURN_STR(BTIF_SM_EXIT_EVT)
    default:
      return "UNKNOWN_EVENT";
  }
}

/*******************************************************************************
  *
  * Function         btif_lpm_sm_update_profile_active_status
  *
  * Description      Sets btif_lpm_is_bt_active_flag and btif_lpm_is_bt_sco_active_flag
  *                  according to the event
  *
  * Parameters       lpm_event - lpm event received in profile update event
  *
  * Returns          None
  *
******************************************************************************/
void btif_lpm_sm_update_profile_active_status(tBTA_LPM_PROFILE_UPDATE_EVT lpm_event) {
  BTIF_TRACE_WARNING("%s: event = %d",__FUNCTION__, lpm_event);
  switch(lpm_event) {
    case BTA_LPM_CONN_DONE:
    case BTA_LPM_DISC_DONE:
      btif_lpm_is_bt_active_flag = false;
      break;
    case BT_LPM_SCO_ACTIVE:
      btif_lpm_is_bt_sco_active_flag = true;
      break;
    case BTA_LPM_CONN_PROGRESS:
    case BTA_LPM_DISC_PROGRESS:
      btif_lpm_is_bt_active_flag = true;
      break;
    case BT_LPM_SCO_INACTIVE:
      btif_lpm_is_bt_sco_active_flag = false;
      break;
    default:
      BTIF_TRACE_WARNING("%s: Invalid event",__FUNCTION__);
  }
}

/*******************************************************************************
 *
 * Function         btif_dispatch_lpm_event
 *
 * Description      Sends message to LPM SM
 *
 * Parameters       event - event to be sent to LPM SM
 *                  p_data - data needed for the event
 *                  len - length of the message
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_dispatch_lpm_event(btif_lpm_sm_event_t event, void *p_data, int len) {
  BTIF_TRACE_EVENT("%s: event=%s", __FUNCTION__,
                  dump_btif_lpm_sm_event_name(event));
  if (lpm_sm_handle != NULL) {
      btif_sm_dispatch(lpm_sm_handle, event, p_data);
  } else {
      BTIF_TRACE_EVENT("%s: state handlers already deinitialized, so ignoring", __FUNCTION__);
  }
}

/*******************************************************************************
 *
 * Function         initialize_lpm_sm
 *
 * Description      Initializes LPM SM
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void initialize_lpm_sm(void) {
  lpm_sm_handle = btif_sm_init(
   (const btif_sm_handler_t*)btif_lpm_sm_state_handlers, BTIF_LPM_STATE_STACK_IDLE, 0);
}

/*******************************************************************************
 *
 * Function         deinitialize_lpm_sm
 *
 * Description      De-initializes LPM SM
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void deinitialize_lpm_sm(void) {
  btif_lpm_native_interface_deinit();
  btif_sm_shutdown(lpm_sm_handle);
  lpm_sm_handle = NULL;
}

/*******************************************************************************
 *
 * Function         lpm_sm_cmd_handler
 *
 * Description      Handler for LPM SM Commands
 *
 * Parameters       Message
 *
 * Returns          None
 *
 ******************************************************************************/
static void lpm_sm_cmd_handler(uint16_t event, char* p_param) {
    BTIF_TRACE_EVENT("%s  event = %d", __FUNCTION__, event);
    //ToDO(in future): Need to check if add prepare offload command to the handler
    switch(event) {
        case BTIF_SM_SEND_FLOW_OFF_CMD:
            btif_lpm_send_flow_off_cmd();
            break;
        case BTIF_SM_SEND_LPM_EXIT_CMD:
            btif_lpm_send_exit_lpm_cmd();
            break;
        case BTIF_SM_SEND_FLOW_ON_CMD:
            btif_lpm_send_flow_on_cmd();
            break;
        case BTIF_SM_SEND_LPM_ENTER_CMD:
            btif_lpm_send_enter_lpm_cmd();
            break;
    }
}

/*******************************************************************************
 *
 * Function         btif_lpm_state_stack_idle_handler
 *
 * Description      LPM SM Stack Idle handler function
 *
 * Parameters       event - LPM SM event to be processed
 *                  data - data needed for the event
 *                  idx - NA
 *
 * Returns          Success or Failure
 *
 ******************************************************************************/
static bool btif_lpm_state_stack_idle_handler(btif_sm_event_t event,
    void* data, int idx) {
  BTIF_TRACE_EVENT("%s event = %s ", __FUNCTION__,
      dump_btif_lpm_sm_event_name((btif_lpm_sm_event_t)event));
  uint8_t status;
  int32_t mode;
  tBTA_LPM_PROFILE_UPDATE_EVT lpm_event;

  switch(event)
  {
    case BTIF_SM_ENTER_EVT:
    case BTIF_SM_EXIT_EVT:
      break;
    case BTIF_LPM_SM_INIT_LPM_PROFILE:
      /* Register LPM Native */
      btif_lpm_native_interface_init();
      break;
    case BTIF_LPM_SM_SEND_FLOW_OFF_CMD:
      /* Send flow off command - VSC */
      btif_transfer_context(lpm_sm_cmd_handler, BTIF_SM_SEND_FLOW_OFF_CMD, NULL, 0, NULL);
      btif_sm_change_state(lpm_sm_handle, BTIF_LPM_STATE_STACK_OFFLOAD_PENDING);
      break;
    case BTIF_LPM_SM_TIMER_ACTIVE_EVT:
      btif_sm_change_state(lpm_sm_handle, BTIF_LPM_STATE_STACK_ACTIVE);
      break;
    case BTIF_LPM_SM_PROFILE_UPDATE_EVT:
      lpm_event = *(tBTA_LPM_PROFILE_UPDATE_EVT*)data;
      BTIF_TRACE_EVENT("%s, event-%d", __FUNCTION__, lpm_event);
      btif_lpm_sm_update_profile_active_status(lpm_event);
      if(btif_lpm_is_bt_active_flag){
        btif_sm_change_state(lpm_sm_handle, BTIF_LPM_STATE_STACK_ACTIVE);
      }
      break;
    case BTIF_LPM_SM_CHECK_OFFLOAD_CMD:
      /* mode from lpm native */
      mode = *(int32_t*)data;
      BTIF_TRACE_EVENT("%s, in BTIF_LPM_SM_CHECK_OFFLOAD_CMD", __FUNCTION__);
      /* tracker mode - set flow off flag, check inactivity timer (VSC)*/
      //if(mode == PwrMode::PWR_MODE_TRK) {
          //btif_lpm_check_timer_activity();
          if(btif_lpm_is_bt_sco_active_flag) {
            /* Sends LPM success, offload failed to LPM Native */
            btif_lpm_native_interface_prepare_offload_done(LPM_SUCCESS,
                          OFFLOAD_FAILED);
          } else {
            /* Send Check offload command VSC */
            btif_lpm_flow_off_flag = true;
            BTA_LPM_ModeSwitchReq(LPM_TRACKER);
            btif_lpm_send_prepare_offload_cmd(btif_lpm_flow_off_flag);
          }
    //}
      /* Send flow off status to stack ?? */
      BTIF_TRACE_EVENT("%s: calling L2cap_flowoff api",__func__);
      L2CA_setLpmFlowoff(true);
      /* twm mode - disconnects all the links except offloaded */
      /* PwrMode::PWR_MODE_ACTIVE */
      break;
    case BTIF_LPM_SM_TIMER_INACTIVE_EVT:
      BTIF_TRACE_EVENT("%s, in BTIF_LPM_SM_TIMER_INACTIVE_EVT", __FUNCTION__);
      break;
    case BTIF_LPM_SM_CHECK_OFFLOAD_ACK_EVT:
      /* extract status */
      BTIF_TRACE_EVENT("%s, in SM_CHECK_OFFLOAD_ACK_EVT", __FUNCTION__);
      status = *(uint8_t*)data;
      /* send context info,offload pending to LPM Native */
      BTIF_TRACE_EVENT("%s, in SM_CHECK_OFFLOAD_ACK_EVT, LPM_PENDING", __FUNCTION__);
      /* Sends LPM pending offload pending to LPM Native */
      btif_lpm_native_interface_prepare_offload_done(LPM_PENDING,
                        OFFLOAD_PENDING);
      /* Send context_info to lpm native  */
      btif_lpm_fetch_ble_context_info_req();
      BTIF_TRACE_EVENT("%s, rfcomm context info req start ..", __FUNCTION__);
      btif_lpm_fetch_rfcomm_context_info_req();
      BTIF_TRACE_EVENT("%s, hci  context info req start ..", __FUNCTION__);
      btif_lpm_fetch_hci_context_info_req();
      BTIF_TRACE_EVENT("%s, l2cap  context info req start ..", __FUNCTION__);
      btif_lpm_fetch_l2cap_context_info_req();

      break;
    case BTIF_LPM_SM_NOTIFY_NACK_MSG:
      /* (yes) reset flow off flag */
      btif_lpm_flow_off_flag = false;
      BTA_LPM_ModeSwitchReq(LPM_ACTIVE);
      btif_lpm_send_prepare_offload_cmd(btif_lpm_flow_off_flag);
      break;
    default:
      BTIF_TRACE_WARNING("%s: Unhandled event=%s",
                         __FUNCTION__,
                         dump_btif_lpm_sm_event_name((btif_lpm_sm_event_t)event));
      return false;
  }
  return true;
}

/*******************************************************************************
 *
 * Function         btif_lpm_state_stack_active_handler
 *
 * Description      LPM SM Stack Active handler function
 *
 * Parameters       event - LPM SM event to be processed
 *                  data - data needed for the event
 *                  idx - NA
 *
 * Returns          Success or Failure
 *
 ******************************************************************************/
static bool btif_lpm_state_stack_active_handler(btif_sm_event_t event,
    void* data, int idx) {
  BTIF_TRACE_DEBUG("%s: event=%s ",
                  __PRETTY_FUNCTION__,
                  dump_btif_lpm_sm_event_name((btif_lpm_sm_event_t)event));

  tBTA_LPM_PROFILE_UPDATE_EVT lpm_event;

  switch (event) {
    case BTIF_SM_ENTER_EVT:
      wakelock_acquire();
      BTIF_TRACE_DEBUG("%s, wakelock acquired",__FUNCTION__);
      //btif_lpm_check_timer_activity();
      break;
    case BTIF_LPM_SM_NOTIFY_NACK_MSG:
      /* Release wakelock if any packet is received */
      BTIF_TRACE_DEBUG("%s, wakelock released",__FUNCTION__);
      wakelock_release();
      /* reset flow off flag */
      btif_lpm_flow_off_flag = false;
      BTA_LPM_ModeSwitchReq(LPM_ACTIVE);
      btif_lpm_send_prepare_offload_cmd(btif_lpm_flow_off_flag);
      break;
    case BTIF_LPM_SM_PROFILE_UPDATE_EVT:
      lpm_event = *(tBTA_LPM_PROFILE_UPDATE_EVT*)data;
      btif_lpm_sm_update_profile_active_status(lpm_event);
      if((!btif_lpm_is_bt_active_flag) && (!btif_lpm_is_bt_sco_active_flag)){
        btif_sm_change_state(lpm_sm_handle, BTIF_LPM_STATE_STACK_IDLE);
      }
      break;
    case BTIF_LPM_SM_TIMER_ACTIVE_EVT:
      /* Sends LPM success, offload failed to LPM Native */
     btif_lpm_native_interface_prepare_offload_done(LPM_SUCCESS,
                        OFFLOAD_FAILED);
      break;
    case BTIF_LPM_SM_CHECK_OFFLOAD_ACK_EVT:
    case BTIF_LPM_SM_CHECK_OFFLOAD_CMD:
      /* send offload failed to LPM Native */
      btif_lpm_native_interface_prepare_offload_done(LPM_FAILED, OFFLOAD_FAILED);
      break;
    case BTIF_LPM_SM_TIMER_INACTIVE_EVT:
      /* Release wakelock if any packet is received */
      BTIF_TRACE_DEBUG("%s, wakelock released",__FUNCTION__);
      btif_sm_change_state(lpm_sm_handle, BTIF_LPM_STATE_STACK_IDLE);
      break;
    case BTIF_SM_EXIT_EVT:
      BTIF_TRACE_DEBUG("%s, wakelock released",__FUNCTION__);
      wakelock_release();
      break;
    default:
      BTIF_TRACE_WARNING("%s: Unhandled event=%s",
                         __PRETTY_FUNCTION__,
                         dump_btif_lpm_sm_event_name((btif_lpm_sm_event_t)event));
      return false;
  }
  return true;
}

/*******************************************************************************
 *
 * Function         btif_lpm_state_stack_active_pending_handler
 *
 * Description      LPM SM Stack Active Pending handler function
 *
 * Parameters       event - LPM SM event to be processed
 *                  data - data needed for the event
 *                  idx - NA
 *
 * Returns          Success or Failure
 *
 ******************************************************************************/
static bool btif_lpm_state_stack_active_pending_handler(btif_sm_event_t event,
    void* data, int idx) {
  BTIF_TRACE_DEBUG("%s: event=%s ",
                  __FUNCTION__,
                  dump_btif_lpm_sm_event_name((btif_lpm_sm_event_t)event));

  switch (event) {
    case BTIF_SM_ENTER_EVT:
    case BTIF_SM_EXIT_EVT:
      break;
    case BTIF_LPM_SM_LPM_EXIT_EVT:
      /* reply success to LPM Native */
      btif_lpm_native_disable_offload_status_interface(LPM_SUCCESS);
      btif_sm_change_state(lpm_sm_handle, BTIF_LPM_STATE_STACK_IDLE);
      break;
    default:
      BTIF_TRACE_WARNING("%s: Unhandled event=%s",
                         __FUNCTION__,
                         dump_btif_lpm_sm_event_name((btif_lpm_sm_event_t)event));
    return false;
  }
  return true;
}

/*******************************************************************************
 *
 * Function         btif_lpm_state_stack_offload_pending_handler
 *
 * Description      LPM SM Stack Offload Pending handler function
 *
 * Parameters       event - LPM SM event to be processed
 *                  data - data needed for the event
 *                  idx - NA
 *
 * Returns          Success or Failure
 *
 ******************************************************************************/
static bool btif_lpm_state_stack_offload_pending_handler(btif_sm_event_t event,
    void* data, int idx) {
  BTIF_TRACE_DEBUG("%s: event=%s ",
                  __FUNCTION__,
                  dump_btif_lpm_sm_event_name((btif_lpm_sm_event_t)event));
  uint8_t flow_on_off_status = 0;
  if(data != NULL){
      flow_on_off_status = *(uint8_t*)data;
  }
  switch (event) {
    case BTIF_SM_ENTER_EVT:
    case BTIF_SM_EXIT_EVT:
      break;
    case BTIF_LPM_SM_LPM_ENTER_ACK_EVT:
      /* reply LPM success to LPM Native */
      btif_lpm_native_interface_commit_offload_done(LPM_SUCCESS, OFFLOAD_PENDING);
      btif_sm_change_state(lpm_sm_handle, BTIF_LPM_STATE_STACK_OFFLOADED);
      break;
    case BTIF_LPM_SM_SEND_LPM_ENTER_CMD:
      /* Send LPM Enter cmd to stack VSC */
      btif_transfer_context(lpm_sm_cmd_handler, BTIF_SM_SEND_LPM_ENTER_CMD, NULL, 0, NULL);
      break;
    case BTIF_LPM_SM_SEND_FLOW_ON_CMD:
      /* Send Flow on to stack VSC */
      btif_transfer_context(lpm_sm_cmd_handler, BTIF_SM_SEND_FLOW_ON_CMD, NULL, 0, NULL);
      break;
    case BTIF_LPM_SM_FLOW_ON_EVT:
      /* extract status */
      /* Send flow on status to LPM Native */
      btif_lpm_native_flow_on_ack_interface(flow_on_off_status);
      break;
    case BTIF_LPM_SM_FLOW_OFF_EVT:
      /* extract status */
      /* Send flow off status to LPM Native */
      btif_lpm_native_flow_off_ack_interface(flow_on_off_status);
      break;
    case BTIF_LPM_SM_SEND_FLOW_OFF_CMD:
      /* make flow off cmd and send (VSC) */
      btif_transfer_context(lpm_sm_cmd_handler, BTIF_SM_SEND_FLOW_OFF_CMD, NULL, 0, NULL);
      break;
    case BTIF_LPM_SM_NOTIFY_NACK_MSG:
      /* Release wakelock if any packet is received */
      BTIF_TRACE_DEBUG("%s, wakelock released",__FUNCTION__);
      wakelock_release();
      /* Reset flow off flag */
      btif_lpm_flow_off_flag = false;
      BTA_LPM_ModeSwitchReq(LPM_ACTIVE);
      btif_lpm_send_prepare_offload_cmd(btif_lpm_flow_off_flag);
      break;
    default:
      BTIF_TRACE_WARNING("%s: Unhandled event=%s",
                         __FUNCTION__,
                         dump_btif_lpm_sm_event_name((btif_lpm_sm_event_t)event));
    return false;
  }
  return true;
}

/*******************************************************************************
 *
 * Function         btif_lpm_state_stack_offloaded_handler
 *
 * Description      LPM SM Stack Offloaded handler function
 *
 * Parameters       event - LPM SM event to be processed
 *                  data - data needed for the event
 *                  idx - NA
 *
 * Returns          Success or Failure
 *
 ******************************************************************************/
static bool btif_lpm_state_stack_offloaded_handler(btif_sm_event_t event,
    void* data, int idx) {
  BTIF_TRACE_DEBUG("%s: event=%s ",
                  __FUNCTION__,
                  dump_btif_lpm_sm_event_name((btif_lpm_sm_event_t)event));

  switch (event) {
    case BTIF_SM_ENTER_EVT:
    case BTIF_SM_EXIT_EVT:
      break;
    case BTIF_LPM_SM_SEND_LPM_EXIT_CMD:
      /* Send LPM Exit command VSC */
      btif_transfer_context(lpm_sm_cmd_handler, BTIF_SM_SEND_LPM_EXIT_CMD, NULL, 0, NULL);
      btif_sm_change_state(lpm_sm_handle, BTIF_LPM_STATE_STACK_ACTIVE_PENDING);
      break;
    default:
      BTIF_TRACE_WARNING("%s: Unhandled event=%s",
                          __FUNCTION__,
                          dump_btif_lpm_sm_event_name((btif_lpm_sm_event_t)event));
    return false;
  }
  return true;
}
