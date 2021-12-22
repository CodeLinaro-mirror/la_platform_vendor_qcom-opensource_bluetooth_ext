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
#include <mutex>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "btif_lpm"
#endif

/*******************************************************************************
 *  Structures and Macros
 ******************************************************************************/
#define BT_STATUS_OK 0x01
#define MAX_OFFLOADBLE_DEVICES 7
#define OFFLOADBLE_APP_MTU_SIZE GATT_DEF_BLE_MTU_SIZE
#define BTIF_LPM_REINIT_START_VAL  10

//Structure to store alarm details
typedef struct {
  char alarm_name[50];
  uint8_t count=0;
}alarm_info_t;

/*******************************************************************************
 *  Static variables
 ******************************************************************************/
static btif_lpm_cb_t btif_lpm_cb;
static bool bt_activity_after_flow_off;
static std::mutex bt_activity_lock;
static bool btif_flow_off_flag;
static std::mutex bt_flow_lock;
static bool wakelock_flag;
static std::mutex wl_lock;

std::vector<alarm_info_t> active_alarm;
std::vector<RawAddress> offloaded_remote_list;

void btif_lpm_evt(tBTA_LPM_EVT event, tBTA_LPM* p_data);

using namespace ::vendor::qti::hardware::bluetooth_offload::V1_0;
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

    //Process context info
    BTIF_TRACE_DEBUG(" : %s processing context info", __func__);
    uint8_t context_length;
    uint8_t version;
    STREAM_TO_UINT8(context_length, stream);
    STREAM_TO_UINT8(version, stream);
    BTIF_TRACE_DEBUG(" : %s Length : 0x%X ",__func__, context_length);
    BTIF_TRACE_DEBUG(" : %s version : 0x%X ",__func__, version);

    //Read and update ACL credits info
    tL2CAP_credits credits;
    STREAM_TO_UINT16(credits.controller_xmit_window, stream);
    STREAM_TO_UINT16(credits.controller_le_xmit_window, stream);
    BTIF_TRACE_DEBUG(" : %s credits.controller_xmit_window : 0x%X ",__func__, credits.controller_xmit_window);
    BTIF_TRACE_DEBUG(" : %s credits.controller_le_xmit_window : 0x%X ",__func__, credits.controller_le_xmit_window);
    L2CA_updateLpmFlowoffCredits(false, credits);

    //Read hci_mode
    uint8_t hci_mode;
    STREAM_TO_UINT8(hci_mode, stream);
    BTIF_TRACE_DEBUG(" : %s hci_mode : 0x%X ",__func__, hci_mode);

    //Read and update RFCOMM port info
    tPORT* port_info = (tPORT*)osi_calloc(sizeof(tPORT));
    uint8_t num_dlci = 0;
    STREAM_TO_UINT8(num_dlci, stream);
    BTIF_TRACE_DEBUG(" : %s num_dlci : 0x%X ",__func__, num_dlci);

    STREAM_TO_UINT8(port_info->dlci, stream);
    STREAM_TO_UINT8(port_info->credit_tx, stream);
    STREAM_TO_UINT8(port_info->credit_rx, stream);

    BTIF_TRACE_DEBUG(" : %s dlci : 0x%X ",__func__, port_info->dlci);
    BTIF_TRACE_DEBUG(" : %s credit_tx : 0x%X ",__func__, port_info->credit_tx);
    BTIF_TRACE_DEBUG(" : %s credit_rx : 0x%X ",__func__, port_info->credit_rx);

    RFCOMM_UpdatePortInfo(port_info);

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

  BTIF_TRACE_DEBUG("btif_lpm_init now going to initialize state machine");
  initialize_lpm_sm();

  btif_dispatch_lpm_event(BTIF_LPM_SM_INIT_LPM_PROFILE,NULL,0);

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

  deinitialize_lpm_sm();
  BTA_DisableLPM();
}

/*******************************************************************************
 *
 * Function         btif_lpm_reinit
 *
 * Description      Re-initializes LPM Profile
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_reinit() {
  BTIF_TRACE_DEBUG(" %s ", __FUNCTION__);

  usleep(BTIF_LPM_REINIT_START_VAL*1000);
  btif_dispatch_lpm_event(BTIF_LPM_SM_REINIT_LPM_NATIVE, NULL, 0);
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
    vendor::qti::hardware::bluetooth_offload::V1_0::btif_lpm_native_set_local_device_address(
      btif_lpm_cb.device_addr, btif_lpm_cb.privacy_enabled);
  }

  BTIF_STORAGE_FILL_PROPERTY(&prop_name, BT_PROPERTY_BDNAME,
                            sizeof(bt_bdname_t), &bdname);

  if (btif_storage_get_adapter_property(&prop_name) == BT_STATUS_SUCCESS) {
    strlcpy(btif_lpm_cb.device_name, (char*) bdname.name, BD_NAME_LEN + 1);
    vendor::qti::hardware::bluetooth_offload::V1_0::btif_lpm_native_set_local_device_name(
      btif_lpm_cb.device_name);
  }
}

/*******************************************************************************
  *
  * Function         btif_lpm_fetch_ble_context_info_req
  *
  * Description      Updates the context info based on the offloaded connections
  *
  * Parameters       None
  *
  * Returns          None
  *
******************************************************************************/
void btif_lpm_fetch_ble_context_info_req(void) {
  bool offloaded_device;

  for (auto it = btif_lpm_cb.ble_conns.begin(); it!=btif_lpm_cb.ble_conns.end(); it++) {
    offloaded_device = false;
    for (auto device : offloaded_remote_list) {
      if (device == it->remote_addr) {
        offloaded_device = true;
        break;
      }
    }
    if (offloaded_device) {
      if(!btif_lpm_native_set_le_context_info(*it, btif_lpm_cb.device_addr))
        BTIF_TRACE_ERROR("Context not set for %s", (it->remote_addr).ToString().c_str());
    }
  }
}

/*******************************************************************************
 *
 * Function         btif_lpm_send_prepare_offload_cmd
 *
 * Description      Function to send Prepare Offload Command to stack
 *
 * Parameters       flow_off_flag - sets flow on/off at hci
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_send_prepare_offload_cmd(bool flow_off_flag) {
  BTIF_TRACE_EVENT("%s", __FUNCTION__);
  uint8_t* msg = (uint8_t *)osi_malloc(DEFAULT_PARAM_LENGTH);
  uint8_t *pp = (uint8_t *)msg;
  *pp++ = flow_off_flag;
  {
   std::unique_lock<std::mutex> lock(bt_flow_lock);
   btif_flow_off_flag = flow_off_flag;
  }
  if (!flow_off_flag) {
    std::unique_lock<std::mutex> lock(bt_activity_lock);
    bt_activity_after_flow_off = false;
    {
      std::unique_lock<std::mutex> lock(wl_lock);
      if (wakelock_flag) {
        wakelock_flag = false;
        wakelock_release();
      }
    }
  }
  BTM_VendorSpecificCommand(HCI_BLE_LPM_PREPARE_OFFLOAD_OPCODE, DEFAULT_PARAM_LENGTH,
          msg, send_prepare_offload_cmd_complete);
  osi_free(msg);
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
  {
  std::unique_lock<std::mutex> lock(bt_activity_lock);
  bt_activity_after_flow_off = false;
  }
  {
  std::unique_lock<std::mutex> lock(bt_flow_lock);
  btif_flow_off_flag = false;
  }
  uint8_t* msg = (uint8_t *)osi_malloc(DEFAULT_PARAM_LENGTH);
  uint8_t *pp = (uint8_t *)msg;
  *pp++ = HCI_BLE_LPM_EXIT_SUB_OPCODE;
  BTM_VendorSpecificCommand(HCI_BLE_LPM_VSC_OPCODE, DEFAULT_PARAM_LENGTH, msg,
    send_exit_lpm_cmd_complete);
  {
    std::unique_lock<std::mutex> lock(wl_lock);
    if (wakelock_flag) {
      wakelock_flag = false;
      wakelock_release();
    }
  }
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
  {
  std::unique_lock<std::mutex> lock(bt_activity_lock);
  btif_flow_off_flag = false;
  }
  {
  std::unique_lock<std::mutex> lock(bt_flow_lock);
  bt_activity_after_flow_off = false;
  }
  uint8_t* msg = (uint8_t *)osi_malloc(DEFAULT_PARAM_LENGTH);
  uint8_t *pp = (uint8_t *)msg;
  *pp++ = HCI_BLE_FLOW_ON_SUB_OPCODE;
  BTM_VendorSpecificCommand(HCI_BLE_LPM_VSC_OPCODE, DEFAULT_PARAM_LENGTH, msg,
    send_flow_on_cmd_complete);
  {
    std::unique_lock<std::mutex> lock(wl_lock);
    if (wakelock_flag) {
      wakelock_flag = false;
      wakelock_release();
    }
  }
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

  if (active_alarm.size()==1)
    is_timer_active = true;

  // Send Timer Active or Timer Inactive Event to SM
  if(is_timer_active) {
      btif_dispatch_lpm_event(BTIF_LPM_SM_TIMER_ACTIVE_EVT,
                  NULL, 0);
  } else {
      btif_dispatch_lpm_event(BTIF_LPM_SM_TIMER_INACTIVE_EVT,
                  NULL, 0);
  }
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
 * Function         lpm_fetch_ble_conn_by_addr
 *
 * Description      Searches BLE connection in LPM CB by address
 *
 * Parameters       addr - Remote address
 *
 * Returns          Iterator to BLE CB
 *
 ******************************************************************************/
std::vector<btif_lpm_ble_conn_t>::iterator lpm_fetch_ble_conn_by_addr
                                                    (const RawAddress& addr) {
  auto it = btif_lpm_cb.ble_conns.begin();
  for (; it!=btif_lpm_cb.ble_conns.end(); it++) {
    if (it->remote_addr == addr)
    break;
  }
  return it;
}

/*******************************************************************************
 *
 * Function         lpm_update_ble_conn_connid
 *
 * Description      Updates BLE conn_id to remote address in LPM CB
 *
 * Parameters       conn_id - Connection id of Remote address
 *                  add - adds(1)/delete(0)
 *
 * Returns          None
 *
 ******************************************************************************/
void lpm_update_ble_conn_connid(RawAddress& addr,uint16_t conn_id, bool add) {
  auto it = btif_lpm_cb.ble_conns.begin();
  for (; it!=btif_lpm_cb.ble_conns.end(); it++) {
    if (it->remote_addr == addr)
      break;
  }
  if (it != btif_lpm_cb.ble_conns.end()) {
    auto find_connid =
         std::find(it->conn_id_list.begin(), it->conn_id_list.end(), conn_id);
    if (add && find_connid == it->conn_id_list.end())
      it->conn_id_list.emplace_back(conn_id);
    else if (!add && find_connid != it->conn_id_list.end())
      it->conn_id_list.erase(find_connid);
    else
      LOG(ERROR) << "Not a valid operation";
  }
}

/*******************************************************************************
 *
 * Function         lpm_fetch_ble_conn_by_connid
 *
 * Description      Searches BLE connection in LPM CB by conn_id
 *
 * Parameters       conn_id - Connection id
 *
 * Returns          Iterator to BLE CB
 *
 ******************************************************************************/
std::vector<btif_lpm_ble_conn_t>::iterator
                     lpm_fetch_ble_conn_by_connid(uint16_t conn_id){
  BTIF_TRACE_DEBUG(" %s: conn_id: %u ", __FUNCTION__, conn_id);
  auto it = btif_lpm_cb.ble_conns.begin();
  for (; it!=btif_lpm_cb.ble_conns.end(); it++) {
    auto find_connid = std::find(it->conn_id_list.begin(), it->conn_id_list.end(), conn_id);
    if (find_connid != it->conn_id_list.end())
      break;
  }
  return it;
}

/*******************************************************************************
 *
 * Function         btif_lpm_ble_conn_srvc_db
 *
 * Description      Stores the service db of ble connection
 *
 * Parameters       conn_id - Remote Address connection id
 *                  db - Pointer to remote gatt db elements
 *                  count - Size of the db elements
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_ble_conn_srvc_db(int conn_id, btgatt_db_element_t* db, int count){
  btif_lpm_ble_conn_t& conn = *lpm_fetch_ble_conn_by_connid((uint16_t)conn_id);
  BTIF_TRACE_DEBUG(" %s: conn id : %d count: %d ", __FUNCTION__, conn_id, count);
  btgatt_db_element_t* curr_db = db;
  for( int i=0; i <count; i++) {
    if(curr_db->type == BTGATT_DB_PRIMARY_SERVICE) {
      BTIF_TRACE_DEBUG(" %s: service uuid : %s ", __FUNCTION__, curr_db->uuid.ToString().c_str());
      conn.gatt_srvc.emplace_back();
      btif_lpm_gatt_t& srvc_el = conn.gatt_srvc.back();
      srvc_el.uuid = curr_db->uuid;
      srvc_el.s_handle= curr_db->start_handle;
      srvc_el.e_handle= curr_db->end_handle;
    }
    curr_db++;
  }
}

/*******************************************************************************
 *
 * Function         btif_lpm_update_ble_conn_evt
 *
 * Description      Updates BLE ACL link to Control Block
 *
 * Parameters       rmt_addr - Remote Address
 *                  rmt_addr_type - Public/Random
 *                  hci_handle - BLE Connection handle
 *                  conn_evt - Connected/Disconnected
 *                  master - Master/Slave
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_update_ble_conn_evt(RawAddress& rmt_addr, bool rmt_addr_type,
           uint16_t hci_handle, bool conn_evt, bool master) {
  BTIF_TRACE_DEBUG(" %s hci_handle:%d conn_evt: %d master %d", __FUNCTION__,
                                               hci_handle, conn_evt, master);
  if (conn_evt) {
    BTIF_TRACE_DEBUG(" %s Add %s to the list", __FUNCTION__,
                                                 rmt_addr.ToString().c_str());
    btif_lpm_cb.ble_conns.emplace_back();
    btif_lpm_ble_conn_t& conn = btif_lpm_cb.ble_conns.back();
    if(master) {
        conn.conn_flags = CONN_ROLE_MASTER;
    } else {
        conn.conn_flags = CONN_ROLE_SLAVE;
    }
    BTIF_TRACE_DEBUG(" %s Master: %d", __FUNCTION__, conn.conn_flags);
    conn.remote_addr = rmt_addr;
    conn.hci_handle = hci_handle;
    /* set mtu size to default */
    conn.mtu = OFFLOADBLE_APP_MTU_SIZE;
    conn.addrtype = rmt_addr_type;
  } else {
    auto it = btif_lpm_cb.ble_conns.begin();
    for (; it!=btif_lpm_cb.ble_conns.end(); it++) {
      if (it->remote_addr == rmt_addr) 
      break;
    }
    btif_lpm_cb.ble_conns.erase(it);
  }
}

/*******************************************************************************
 *
 * Function         btif_lpm_update_gattc_upstreams_evt
 *
 * Description      Updates gatt client events to LPM
 *
 * Parameters       event - GATT Client event
 *                  p_data - data associated with the event
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_update_gattc_upstreams_evt(tBTA_GATTC_EVT event, tBTA_GATTC* p_data) {
  BTIF_TRACE_DEBUG(" %s: event : %d ", __FUNCTION__, event);
  switch (event) {
    case BTA_GATTC_OPEN_EVT: {
    if (p_data->open.status == GATT_SUCCESS)
      lpm_update_ble_conn_connid(p_data->open.remote_bda, p_data->open.conn_id, true);
      if (GATT_DEF_BLE_MTU_SIZE != p_data->open.mtu && p_data->open.mtu) {
        btif_lpm_ble_conn_t& conn = *lpm_fetch_ble_conn_by_connid(p_data->open.conn_id);
        conn.mtu = p_data->open.mtu;
      }
      break;
    }
    case BTA_GATTC_CLOSE_EVT: {
    if (p_data->status == GATT_SUCCESS)
        lpm_update_ble_conn_connid(p_data->close.remote_bda,p_data->close.conn_id, false);
      break;
    }
    case BTA_GATTC_CFG_MTU_EVT: {
    if (p_data->cfg_mtu.status == GATT_SUCCESS) {
      btif_lpm_ble_conn_t& conn = *lpm_fetch_ble_conn_by_connid(p_data->cfg_mtu.conn_id);
      conn.mtu = p_data->cfg_mtu.mtu;
      BTIF_TRACE_DEBUG("%s, updated mtu size:%d", __FUNCTION__, conn.mtu);
      }
      break;
    }
    default:
      break;
  }
}

/*******************************************************************************
 *
 * Function         btif_lpm_gattc_inform_notify_reg
 *
 * Description      Informs Notification Registration to LPM
 *
 * Parameters       addr - Remote Address
 *                  handle - handle registered
 *                  reg - registered(1)/de-registered(0)
 *                  status - GATT status
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_gattc_inform_notify_reg(const RawAddress& addr,uint16_t handle,
                                               bool reg, tGATT_STATUS status) {
  BTIF_TRACE_DEBUG(" %s ", __FUNCTION__);
  if(status == GATT_SUCCESS) {
    btif_lpm_ble_conn_t& conn = *lpm_fetch_ble_conn_by_addr(addr);
    for(auto srvc_it = conn.gatt_srvc.begin();
                           srvc_it != conn.gatt_srvc.end(); srvc_it++) {
      if (srvc_it->s_handle <= handle && srvc_it->s_handle >= handle) {
        srvc_it->is_notify_reg = reg;
        break;
      }
    }
  }
}

/*******************************************************************************
 *
 * Function         btif_lpm_update_adapter_property
 *
 * Description      Updates adapter properties in LPM CB
 *
 * Parameters       p_prop - Adapter Property modified
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_update_adapter_property(bt_property_t* p_prop) {
  BTIF_TRACE_DEBUG(" %s ", __FUNCTION__);
  uint8_t name_len;
    if (p_prop->type == BT_PROPERTY_BDNAME) {
      name_len = p_prop->len > BTM_MAX_LOC_BD_NAME_LEN
                     ? BTM_MAX_LOC_BD_NAME_LEN
                     : p_prop->len;
      strlcpy(btif_lpm_cb.device_name,(char*) p_prop->val, name_len+1);
    }
  /* update lpm native interface */
}

/*******************************************************************************
 *
 * Function         btif_lpm_update_gatts_service
 *
 * Description      Save context info to control block 
 *
 * Parameters       num_attr - Number of elements
 *                  srvc_db_elem - Pointer to db
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_update_gatts_service(uint8_t num_attr, tLPM_GATTS_SRVC_ATTR* srvc_db_elem) {
  if (!num_attr)
    return;

  for(int i = 0; i < num_attr; i++, srvc_db_elem++) {
    btif_lpm_cb.gatts_db.push_back(*srvc_db_elem);
  }
}

bool btif_lpm_check_remote_addr(RawAddress& bd_addr) {
  BTIF_TRACE_DEBUG("%s, addr : %s", __FUNCTION__,bd_addr.ToString().c_str());
   if (strcmp(bd_addr.ToString().c_str(),"ff:ff:ff:ff:ff:ff")) {
       BTIF_TRACE_DEBUG("%s, valid address found returing true", __FUNCTION__);
       return true;
   } else {
       BTIF_TRACE_ERROR("%s, invalid address returning false", __FUNCTION__);
       return false;
   }
}

void btif_lpm_update_rfcomm_context(tBTA_LPM *p_rfcomm){
  BTIF_TRACE_DEBUG("%s, rfcomm context info and Connection event %d", __FUNCTION__,  p_rfcomm->rfcomm_evt.conn_evt);
  if (!(btif_lpm_check_remote_addr(p_rfcomm->rfcomm_evt.port_info.bd_addr))) {
      BTIF_TRACE_ERROR("%s, rfcomm context info with bad address", __FUNCTION__);
      return;
  }
  if (p_rfcomm->rfcomm_evt.conn_evt == BTA_LPM_SOCKET_CONN_OPEN) {
      btif_rfcomm_context_t p_context;
      p_context.dlci = p_rfcomm->rfcomm_evt.port_info.dlci;
      p_context.remote_addr = p_rfcomm->rfcomm_evt.port_info.bd_addr;
      p_context.localCredits = p_rfcomm->rfcomm_evt.port_info.credit_tx;
      p_context.remoteCredits = p_rfcomm->rfcomm_evt.port_info.credit_rx;
      p_context.mtu = p_rfcomm->rfcomm_evt.port_info.mtu;
      if (p_rfcomm->rfcomm_evt.port_info.is_server) {
          p_context.flags = IS_SERVER;
      } else {
          p_context.flags = IS_CLIENT;
      }
      BTIF_TRACE_DEBUG("%s, p_context.dlci: %d, flags %d", __FUNCTION__,p_context.dlci,p_context.flags);
      btif_lpm_cb.comm_context.emplace_back(p_context);
  } else {
      for(auto it =  btif_lpm_cb.comm_context.begin();it != btif_lpm_cb.comm_context.end();++it){
          if (p_rfcomm->rfcomm_evt.port_info.dlci == it->dlci) {
              BTIF_TRACE_DEBUG("%s, clearing port with dlci: %d", __FUNCTION__,it->dlci);
              btif_lpm_cb.comm_context.erase(it);
              if (btif_lpm_cb.comm_context.size() == 1) {
                  BTIF_TRACE_DEBUG("%s, clearing multiplexer port with dlci: %d", __FUNCTION__,btif_lpm_cb.comm_context.front().dlci);
                  btif_lpm_cb.comm_context.clear();
                  btif_lpm_cb.l2cap_context.clear();
                  btif_lpm_cb.bredr_context.clear();
              }
              break;
          }
      }
  }
}

void btif_lpm_update_hci_info(tBTA_LPM *p_hci){
  BTIF_TRACE_ERROR("%s, hci context info", __FUNCTION__);
  btif_hci_context_t p_context;
  p_context.remote_addr = p_hci->hci_evt.bd_addr;
  BTIF_TRACE_ERROR("%s, hci context info addr : %s", __FUNCTION__,p_context.remote_addr.ToString().c_str());
  p_context.hci_handle = p_hci->hci_evt.hci_info.hci_handle;
  p_context.role = p_hci->hci_evt.hci_info.role;
  btif_lpm_cb.bredr_context.emplace_back(p_context);
}

void btif_lpm_update_l2cap_info(tBTA_LPM *p_l2cap){
  BTIF_TRACE_DEBUG("%s, hci context info", __FUNCTION__);
  btif_l2cap_context_t p_context;
  p_context.remote_addr = p_l2cap->l2cap_cfg_evt.bd_addr;
  BTIF_TRACE_DEBUG("%s, hci context info addr : %s", __FUNCTION__,p_context.remote_addr.ToString().c_str());
  p_context.psm = p_l2cap->l2cap_cfg_evt.l2cap_info.psm;
  p_context.local_cid = p_l2cap->l2cap_cfg_evt.l2cap_info.local_cid;
  p_context.remote_cid = p_l2cap->l2cap_cfg_evt.l2cap_info.remote_cid;
  p_context.local_mtu = p_l2cap->l2cap_cfg_evt.l2cap_info.local_mtu;
  p_context.remote_mtu = p_l2cap->l2cap_cfg_evt.l2cap_info.remote_mtu;
  btif_lpm_cb.l2cap_context.emplace_back(p_context);
}

void btif_lpm_update_rfcomm_credits(tBTA_LPM *rfcomm_credits) {
   //to do based on dlsi value need to update that rfcomm context
   BTIF_TRACE_DEBUG("%s, dlci %d, credits %d, is_tx_credits %d", __FUNCTION__, rfcomm_credits->rfcomm_crd_evt.credit_info.dlci, rfcomm_credits->rfcomm_crd_evt.credit_info.credits, rfcomm_credits->rfcomm_crd_evt.credit_info.is_tx_credits);
   uint8_t dlci = rfcomm_credits->rfcomm_crd_evt.credit_info.dlci;
   for(auto it =  btif_lpm_cb.comm_context.begin();it != btif_lpm_cb.comm_context.end();++it){
	   if (dlci == it->dlci) {
		   BTIF_TRACE_DEBUG("%s, DLCI found and Updating credits", __FUNCTION__);
		   if (rfcomm_credits->rfcomm_crd_evt.credit_info.is_tx_credits) {
			   it->localCredits = rfcomm_credits->rfcomm_crd_evt.credit_info.credits;
		   } else {
			   it->remoteCredits = rfcomm_credits->rfcomm_crd_evt.credit_info.credits;
		   }
		   break;
	   }
   }
}

/*******************************************************************************
 *
 * Function         btif_lpm_prepare_offload_info
 *
 * Description      LPM native offload information
 *
 * Parameters       mode - LPM Mode requested
 *                  remote_devices - List of offloadable devices
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_prepare_offload_info(tLPM_mode mode,
                                      std::vector<RawAddress> remote_devices) {
  BTIF_TRACE_DEBUG("%s", __FUNCTION__);
  /* Inform BTA layer regarding offloadable connections */
  BTA_DmSetLPMInfo(mode, remote_devices);

  /* Store the new list of remote devices - used for fetching context info*/
  offloaded_remote_list.assign(remote_devices.begin(), remote_devices.end());
  for(auto it =  remote_devices.begin();it != remote_devices.end();++it) {
	BTIF_TRACE_DEBUG("%s, remote_devices address %s", __FUNCTION__, it->ToString().c_str());
  }
}

/*******************************************************************************
 *

 * Function         btif_lpm_alarm_start
 *
 * Description      Updates alarm list
 *
 * Parameters       alarm_name - Alarm name
 *
 * Returns          None
 *
 ******************************************************************************/

void btif_lpm_alarm_start(char* alarm_name) {
  /* Check if alarm already present in list*/
  BTIF_TRACE_DEBUG("alarm_type %s",alarm_name);
  if (btif_flow_off_flag && !strcmp(alarm_name,"hci.command_response_timer"))
    return;
  if(!strcmp(alarm_name, "btif.config")) {
    /*do nothing*/
    BTIF_TRACE_DEBUG("btif.config");
    return;
  }
  std::vector<alarm_info_t>::iterator it;
  for(it = active_alarm.begin(); it != active_alarm.end(); it++) {
    if(!strcmp(alarm_name, it->alarm_name)) {
        it->count++;
        break;
    }
  }

  /* If alarm is not present add to list */
  if((it == active_alarm.end()) && (strcmp(alarm_name, "btif.config"))) {
    alarm_info_t tmp;
    strlcpy(tmp.alarm_name, alarm_name, (strlen(alarm_name)+1));
    tmp.count = 1;
    active_alarm.push_back(tmp);
    if (active_alarm.size()==1)
      btif_lpm_check_timer_activity();
    BTIF_TRACE_DEBUG(" %s, adding alarm: %s size: %d", __FUNCTION__, tmp.alarm_name,
                                                          active_alarm.size());
  }
}

/*******************************************************************************
 *
 * Function         btif_lpm_alarm_stop
 *
 * Description      Updates alarm list
 *
 * Parameters       alarm_name - Alarm name
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_alarm_stop(char *alarm_name) {
  std::vector<alarm_info_t>::iterator it;
  for(it = active_alarm.begin(); it != active_alarm.end(); it++) {
    if(!strcmp(alarm_name, it->alarm_name)) {
      BTIF_TRACE_DEBUG(" %s, found alarm count = %d ",
                                    __FUNCTION__, it->count);
      it->count--;
      if(it->count == 0) {
        active_alarm.erase(it);
        if (active_alarm.size()==0)
          btif_lpm_check_timer_activity();
      }
      break;
    }
  }
}

/*******************************************************************************
 *
 * Function         btif_lpm_update_credits
 *
 * Description      Update L2CAP buffers
 *
 * Parameters       L2cap event structure
 *
 * Returns          None
 *
 ******************************************************************************/

void btif_lpm_update_credits(tBTA_LPM_L2CAP_EVT *l2cap_evt) {

   BTIF_TRACE_ERROR("%s, l2cal credits - le credits:%d", __FUNCTION__,
                                    l2cap_evt->credit_info.controller_le_xmit_window);
   vendor::qti::hardware::bluetooth_offload::V1_0::btif_lpm_native_interface_set_Hci_Acl_Credits(BtType::BT_TYPE_LE,
                                              l2cap_evt->credit_info.controller_le_xmit_window);
   vendor::qti::hardware::bluetooth_offload::V1_0::btif_lpm_native_interface_set_Hci_Acl_Credits(BtType::BT_TYPE_BT_EDR,
                                              l2cap_evt->credit_info.controller_xmit_window);
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
    case LPM_BLE_CONN_EVT: {
      btif_lpm_update_ble_conn_evt(p_data->ble_conn.rmt_addr, p_data->ble_conn.rmt_addr_type,
           p_data->ble_conn.hci_handle, p_data->ble_conn.conn_evt, p_data->ble_conn.master);
      break;
    }
    case LPM_GATTS_EVT: {
      /* Save context info to control block */
      btif_lpm_update_gatts_service(p_data->gatts_srvc.num_attr, p_data->gatts_srvc.srvc_attr);
      break;
    }
    case LPM_ALARM_EVT: {
      tBTA_LPM *alarm = (tBTA_LPM*)p_param;
      if (alarm->alarm_evt.state == ALARM_START)
        btif_lpm_alarm_start(alarm->alarm_evt.name);
      else if (alarm->alarm_evt.state == ALARM_STOP)
        btif_lpm_alarm_stop(alarm->alarm_evt.name);
      break;
    }
    case LPM_L2CAP_EVT: {
      tBTA_LPM_L2CAP_EVT *l2cap_evt = (tBTA_LPM_L2CAP_EVT*)p_param;
      btif_lpm_update_credits(l2cap_evt);
      break;
    }
    case LPM_PACKET_AFTER_FLOW_OFF: {
      {
        std::unique_lock<std::mutex> lock(bt_activity_lock);
        bt_activity_after_flow_off = true;
      }
      {
        std::unique_lock<std::mutex> lock(wl_lock);
        if (!wakelock_flag) {
          wakelock_flag = true;
          wakelock_acquire();
        }
      }
      break;
    }
    case LPM_RFCOMM_EVT:
      LOG(ERROR) << "LPM_RFCOMM_EVT";
      BTIF_TRACE_DEBUG("%s p_data.rfcomm_evt.port_info.dlci: %d",__FUNCTION__,p_data->rfcomm_evt.port_info.dlci);
      btif_lpm_update_rfcomm_context(p_data);
      break;
    case LPM_HCI_INFO_EVT:
      LOG(ERROR) << "LPM_HCI_INFO_EVT";
      BTIF_TRACE_DEBUG("%s p_data.hci_evt.hci_info.bd_addr: %s",__FUNCTION__,p_data->hci_evt.bd_addr.ToString().c_str());
      btif_lpm_update_hci_info(p_data);
      break;
    case LPM_GATT_REMOTE_SRVC_DB_EVT:
      LOG(ERROR) << "LPM_GATT_REMOTE_SRVC_DB_EVT";
      BTIF_TRACE_DEBUG("%s p_data.rmt_srvc_db_evt.hci_handle: %d",__FUNCTION__,p_data->rmt_srvc_db_evt.hci_handle);
      btif_lpm_ble_conn_srvc_db(p_data->rmt_srvc_db_evt.hci_handle, p_data->rmt_srvc_db_evt.db_element, p_data->rmt_srvc_db_evt.num_attr);
      break;
    case LPM_L2CAP_INFO_EVT:
      LOG(ERROR) << "LPM_L2CAP_INFO_EVT";
      BTIF_TRACE_DEBUG("%s p_data.l2cap_cfg_evt.l2cap_info.bd_addr: %s",__FUNCTION__,p_data->l2cap_cfg_evt.bd_addr.ToString().c_str());
      btif_lpm_update_l2cap_info(p_data);
      break;
    case LPM_RFCOMM_CREDIT_EVT:
      LOG(ERROR) << "LPM_RFCOMM_CREDIT_EVT";
      btif_lpm_update_rfcomm_credits(p_data);
      break;
    default:
      break;
  }
}

/*******************************************************************************
 *
 * Function         is_bt_active_during_offload
 *
 * Description      Informs Notification Registration to LPM
 *
 * Parameters       None
 *
 * Returns          bool - status of BT Tx at hci/l2cap after flow off
 *
 ******************************************************************************/
bool is_bt_active_during_offload(void) {
  std::unique_lock<std::mutex> lock(bt_activity_lock);
  return bt_activity_after_flow_off;
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

/*******************************************************************************
 *
 * Function         btif_lpm_fetch_rfcomm_context_info_req
 *
 * Description      Callback function
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void  btif_lpm_fetch_rfcomm_context_info_req(){
  BTIF_TRACE_DEBUG("%s", __FUNCTION__);
  bool offloaded_device;

  for (auto it = btif_lpm_cb.comm_context.begin(); it!=btif_lpm_cb.comm_context.end(); it++) {
    offloaded_device = false;
    for (auto device : offloaded_remote_list) {
      if (device == it->remote_addr) {
        offloaded_device = true;
        break;
      }
    }
    if (offloaded_device) {
      BTIF_TRACE_EVENT("%s, offload_device = true", __FUNCTION__);
      if(!vendor::qti::hardware::bluetooth_offload::V1_0::btif_lpm_native_interface_set_rfcomm_context_info(btif_lpm_cb.comm_context, btif_lpm_cb.l2cap_context.back().remote_addr.ToString()))
        BTIF_TRACE_ERROR("%s, couldn't send context info to lpm native", __FUNCTION__);
    }
  }
 }

void btif_lpm_fetch_hci_context_info_req(){
  BTIF_TRACE_EVENT("%s, before lpm_native_set context info", __FUNCTION__);
  bool offloaded_device;

  for (auto it = btif_lpm_cb.bredr_context.begin(); it!=btif_lpm_cb.bredr_context.end(); it++) {
    offloaded_device = false;
    for (auto device : offloaded_remote_list) {
      if (device == it->remote_addr) {
        offloaded_device = true;
        break;
      }
    }
    if (offloaded_device) {
      BTIF_TRACE_EVENT("%s, offload_device = true", __FUNCTION__);
      if(!vendor::qti::hardware::bluetooth_offload::V1_0::btif_lpm_native_interface_set_hci_context_info((it->remote_addr).ToString(),*it)) {
        BTIF_TRACE_ERROR("Context not set for %s", (it->remote_addr).ToString().c_str());
      } else {
        BTIF_TRACE_DEBUG("Context set for %s", (it->remote_addr).ToString().c_str());
      }
	}
  }
}

void  btif_lpm_fetch_l2cap_context_info_req(){
  BTIF_TRACE_DEBUG("%s", __FUNCTION__);
  bool offloaded_device;

  for (auto it = btif_lpm_cb.l2cap_context.begin(); it!=btif_lpm_cb.l2cap_context.end(); it++) {
    offloaded_device = false;
    for (auto device : offloaded_remote_list) {
      if (device == it->remote_addr) {
        offloaded_device = true;
        break;
      }
    }
    if (offloaded_device) {
      BTIF_TRACE_EVENT("%s, offload_device = true", __FUNCTION__);
      if(!vendor::qti::hardware::bluetooth_offload::V1_0::btif_lpm_native_interface_set_l2cap_context_info(btif_lpm_cb.l2cap_context, btif_lpm_cb.l2cap_context.back().remote_addr.ToString()))
        BTIF_TRACE_ERROR("%s, couldn't send context info to lpm native", __FUNCTION__);
    }
  }
}
