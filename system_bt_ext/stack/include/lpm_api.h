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
 *  Filename:      lpm_api.h
 *
 *  Description:   Stack interface file for LPM Profile
 *
 ******************************************************************************/

#ifndef LPM_API_H
#define LPM_API_H

#include "bt_types.h"
#include "gatt_api.h"
#include "stack/rfcomm/port_int.h"
#include "stack/include/l2c_api.h"

/*****************************************************************************
 *  Constants
 ****************************************************************************/

typedef struct {
  bluetooth::Uuid attr_uuid;
  uint16_t handle;
  uint16_t offset;
  uint16_t len;
  uint8_t value[GATT_MAX_ATTR_LEN];
  bt_gatt_db_attribute_type_t gatt_type;
} tLPM_GATTS_SRVC_ATTR;

typedef struct {
  uint8_t state;
  uint8_t scn;
  uint16_t uuid;
  RawAddress bd_addr;
  bool is_server;
  uint8_t dlci;
  uint16_t mtu;
  uint16_t peer_mtu;
  uint16_t credit_tx;
  uint16_t credit_rx;
  uint16_t credit_rx_max;
  uint16_t credit_rx_low;
  uint16_t keep_mtu;
} tLPM_PORT_INFO;

typedef struct{
  uint32_t hci_handle;
  uint32_t mode;
  uint32_t role;
  uint32_t direction;
} tLPM_HCI_INFO;

/*****************************************************************************
 *  Callbacks
 ****************************************************************************/

typedef void (tLPM_BLE_CONN_CBACK) (const RawAddress& rmt_addr, bool addr_type,
                                     bool master, bool connected, uint16_t hci_handle);

typedef void (tLPM_GATTS_DB_ELEM_CBACK) (std::vector<tLPM_GATTS_SRVC_ATTR> attr_list);

typedef void (tLPM_PACKET_AFTER_FLOW_OFF) (void);

typedef void (tLPM_RFCOMM_PORT_INFO_CBACK) (tLPM_PORT_INFO *port_info);

typedef void (tLPM_HCI_INFO_CBACK) (tLPM_HCI_INFO *hci_info,const RawAddress& rmt_addr);

typedef void (tLPM_L2CAP_INFO_CBACK) (tL2CAP_credits credit_info);

typedef struct {
  tLPM_BLE_CONN_CBACK* p_ble_conn_cb;
  tLPM_GATTS_DB_ELEM_CBACK* p_gatts_service_cb;
  tLPM_PACKET_AFTER_FLOW_OFF* p_packet_after_flow_off;
  tLPM_RFCOMM_PORT_INFO_CBACK* p_rfcomm_port_info_cb;
  tLPM_HCI_INFO_CBACK* p_hci_info_cb;
  tLPM_L2CAP_INFO_CBACK* P_l2cap_credit_info_cb;
} tLPM_CBACK;


 /*******************************************************************************
 *
 * Function         LPM_Enable
 *
 * Description      This function is called to register LPM with stack
 *
 * Parameter        p_cb: callback functions.
 *
 * Returns          None
 *
 ******************************************************************************/
extern void LPM_Enable(tLPM_CBACK* p_cb);

 /*******************************************************************************
 *
 * Function         LPM_ModeSwitchReq
 *
 * Description      This function is used to switch the stack mode
 *
 * Parameter        mode: Desired mode to switch
 *
 * Returns          None
 *
 ******************************************************************************/
extern void LPM_ModeSwitchReq(tLPM_mode mode);

extern void LPM_CopyPortInfo(tPORT*);

extern void LPM_CopyHCIInfo(uint16_t,uint8_t,const RawAddress&);

extern void LPM_CopyL2CAPCredits(tL2CAP_credits credits);

#endif /* LPM_API_H */
