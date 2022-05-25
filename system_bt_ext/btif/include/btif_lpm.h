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
 *  Filename:      btif_lpm.h
 *
 *  Description:   Header file for all interface apis and internal apis of btif
 *
 ******************************************************************************/

#ifndef BTIF_LPM_H
#define BTIF_LPM_H

#include "btif_sm.h"
#include "btif_dm.h"
#include "bta_gatt_api.h"
#include "hardware/vendor.h"
#include <hardware/bluetooth.h>

#include "btif_lpm_sm.h"
#include "bta_lpm_api.h"

#include "bt_common.h"
#include "bt_utils.h"
#include "bt_target.h"
#include "bta/include/bta_api.h"
#include "btif_util.h"
#include "btu.h"
#include "osi/include/allocator.h"
#include "osi/include/osi.h"
#include "osi/include/properties.h"
#include "btif_storage.h"
#include "hcidefs.h"

/*****************************************************************************
 *  Structures and Macros
 ****************************************************************************/
#define DEFAULT_PARAM_LENGTH 0x01
#define CONN_ROLE_MASTER 0x01
#define CONN_ROLE_SLAVE  0x02

#define IS_SERVER 0
#define IS_CLIENT 1

typedef struct {
  bluetooth::Uuid uuid;
  uint16_t desc_handle;
} btif_lpm_desc_t;

typedef struct {
  bluetooth::Uuid uuid;
  uint16_t char_handle;
  uint16_t char_property;
  std::string char_value;
  std::vector<btif_lpm_desc_t> gatt_descs;
} btif_lpm_char_info_t;

typedef struct {
  bluetooth::Uuid uuid;
  uint16_t s_handle;
  uint16_t e_handle;
  bool is_notify_reg;
  std::vector<btif_lpm_char_info_t> gatt_chars;
} btif_lpm_gatt_t;

typedef struct {
  RawAddress remote_addr;
  uint16_t hci_handle;
  bool in_use;
  uint16_t conn_flags;
  uint16_t mtu;
  bool srvc_bit_set;
  bool addrtype;
  uint16_t remote_gatt_srvc_handle;
  std::vector<btif_lpm_gatt_t> gatt_srvc;
  std::vector<uint16_t> conn_id_list;
} btif_lpm_ble_conn_t;

/* BTIF LPM Control Block */
typedef struct {
  uint8_t is_lpm_registered;
  RawAddress remote_addr;
  uint8_t dlci;
  uint8_t localCredits;
  uint8_t remoteCredits;
  uint16_t mtu;
  uint8_t flags;
} btif_rfcomm_context_t;

typedef struct {
  RawAddress remote_addr;
  uint32_t hci_handle;
  uint32_t role;
  uint32_t mode;
  uint32_t direction;
} btif_hci_context_t;

typedef struct {
  RawAddress remote_addr;
  uint16_t psm;
  uint16_t local_cid;
  uint16_t remote_cid;
  uint16_t local_mtu;
  uint16_t remote_mtu;
} btif_l2cap_context_t;

/* BTIF LPM Control Block */
typedef struct {
  bool privacy_enabled;
  std::string device_addr;
  char device_name[BD_NAME_LEN + 1];
  uint8_t logging_byte;
  std::vector<btif_lpm_ble_conn_t> ble_conns;
  std::vector<tLPM_GATTS_SRVC_ATTR> gatts_db;
  std::vector<btif_rfcomm_context_t> comm_context;
  std::vector<btif_hci_context_t> bredr_context;
  std::vector<btif_l2cap_context_t> l2cap_context;
} btif_lpm_cb_t;

typedef struct {
    uint8_t ltk[16];
    uint8_t rand[8];
    uint16_t ediv;
    uint8_t sec_level;
    uint8_t key_size;
} tBTA_LE_KEY_VALUE_local;

typedef struct {
  uint8_t ir[16];
  uint8_t er[16];
} btif_dm_local_key_id_t_local;

typedef struct {
    uint8_t link_key[16];
} btif_lpm_bredr_link_key;

typedef enum {
    BTAV_LPM_AUDIO_STATE_REMOTE_SUSPEND = 0,
    BTAV_LPM_AUDIO_STATE_STOPPED,
    BTAV_LPM_AUDIO_STATE_STARTED,
} btav_lpm_audio_state_t;

typedef enum {
    BTHF_LPM_AUDIO_STATE_DISCONNECTED = 0,
    BTHF_LPM_AUDIO_STATE_CONNECTING,
    BTHF_LPM_AUDIO_STATE_CONNECTED,
    BTHF_LPM_AUDIO_STATE_DISCONNECTING
} bthf_audio_state_t;

/*****************************************************************************
 *  BTIF LPM Function Declarations
 ****************************************************************************/

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
void btif_lpm_init(void);

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
void btif_lpm_init_done(void);

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
void btif_lpm_deinit(void);

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
void btif_lpm_reinit(void);

/*******************************************************************************
 *
 * Function         btif_lpm_send_prepare_offload_cmd
 *
 * Description      Function to send Prepare Offload Command to stack
 *
 * Parameters       flow on/off - sets flow off at hci layer
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_send_prepare_offload_cmd(bool flow_off);

/*******************************************************************************
 *
 * Function         btif_lmp_dm_link_down_event
 *
 * Description      Function will get link down event
 *
 * Parameters       num_active_br_edr_links: Number of active br edr links
 *                  num_active_le_links : Number of active le links
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lmp_dm_link_down_event(uint16_t num_active_br_edr_links, uint16_t num_active_le_links);


/*******************************************************************************
 *
 * Function         btif_lpm_check_offloadable_links
 *
 * Description      Function will check if there are any offloadable links or not
 *
 * Parameters       None
 *
 * Returns          true if there are only offloadable links,
                    false if there are non offloadable links also.
 *
 ******************************************************************************/
bool btif_lpm_check_offloadable_links(void);

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
void btif_lpm_send_enter_lpm_cmd(void);

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
void btif_lpm_send_exit_lpm_cmd(void);

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
void btif_lpm_send_flow_on_cmd(void);

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
void btif_lpm_send_flow_off_cmd(void);

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
void btif_lpm_check_timer_activity(void);

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
void btif_lpm_fetch_ble_context_info_req(void);

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
void btif_lpm_update_gattc_upstreams_evt(tBTA_GATTC_EVT event, tBTA_GATTC* p_data);

/*******************************************************************************
 *
 * Function         btif_lpm_update_gatts_handle_event
 *
 * Description      Updates gatt server events to LPM
 *
 * Parameters       event - GATT Server event
 *                  p_data - data associated with the event
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_update_gatts_handle_event(tBTA_GATTS_EVT event, tBTA_GATTS* p_data);

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
void btif_lpm_ble_conn_srvc_db(int conn_id, btgatt_db_element_t* db, int count);

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
void btif_lpm_update_adapter_property(bt_property_t* p_prop);

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
void btif_lpm_gattc_inform_notify_reg(const RawAddress& addr, uint16_t handle,
                                                   bool reg, tGATT_STATUS status);

/*******************************************************************************
 *
 * Function         btif_lpm_prepare_offload_info
 *
 * Description      LPM native offload information
 *
 * Parameters       mode - LPM Mode requested
 *                  remote_devices - List of offloadable devices
 *
 * Returns          status - success or fail
 *
 ******************************************************************************/
int btif_lpm_prepare_offload_info(tLPM_mode mode,
                                         std::vector<RawAddress> remote_devices);

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
bool is_bt_active_during_offload(void);
void  btif_lpm_update_rfcomm_context(tBTA_LPM*);
void btif_lpm_fetch_rfcomm_context_info_req();
void btif_lpm_fetch_hci_context_info_req();
void btif_lpm_fetch_l2cap_context_info_req();
void btif_lpm_a2dp_on_stream_update(btav_lpm_audio_state_t state);
void btif_lpm_hf_audio_update(bthf_audio_state_t state);
#endif /* BTIF_LPM_H */
