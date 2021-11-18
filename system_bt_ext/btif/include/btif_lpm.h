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

typedef struct {
  bluetooth::Uuid uuid;
  uint16_t s_handle;
  uint16_t e_handle;
  bool is_notify_reg;
} btif_lpm_gatt_t;

typedef struct {
  RawAddress remote_addr;
  uint16_t hci_handle;
  bool in_use;
  uint16_t conn_flags;
  uint8_t mtu;
  bool srvc_bit_set;
  uint16_t remote_gatt_srvc_handle;
  std::vector<btif_lpm_gatt_t> gatt_srvc;
  std::vector<uint16_t> conn_id_list;
} btif_lpm_ble_conn_t;

/* BTIF LPM Control Block */
typedef struct {
  bool privacy_enabled;
  std::string device_addr;
  char device_name[BD_NAME_LEN + 1];
  uint8_t logging_byte;
  std::vector<btif_lpm_ble_conn_t> ble_conns;
  std::vector<tLPM_GATTS_SRVC_ATTR> gatts_db;
} btif_lpm_cb_t;

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
#endif /* BTIF_LPM_H */
