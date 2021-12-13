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

/******************************************************************************
 *
 *  This is the implementation file of STACK LPM
 *
 ******************************************************************************/
#include "lpm_api.h"
#include "lpm_int.h"

#include <base/logging.h>
#include "btm_int_types.h"
#include "btm_int.h"
#include "osi/include/log.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "bt_lpm"
#endif

using bluetooth::Uuid;

/*****************************************************************************
 *  Structures and static
 ****************************************************************************/

typedef struct {
  uint16_t ble_handle;
  RawAddress rmt_addr;
  uint8_t master;
}tLPM_BLE_CONN;

/* LPM Control Block */
typedef struct {
  tLPM_CBACK lpm_cback;
  tLPM_mode mode;
  bool flow_off;
  bool refresh_raddr_timer_state;
  std::vector<tLPM_BLE_CONN> ble_conn;
  std::vector<tLPM_GATTS_SRVC_ATTR> srvc_list;
} tLPM_CB;

/* Control Block */
tLPM_CB lpm_cb;

extern tGATT_STATUS read_gap_attr_value(uint16_t handle, tGATT_VALUE* p_value,
                              bool is_long);

static std::vector<tLPM_GATTS_SRVC_ATTR> srvc_attr_list;

 /*******************************************************************************
 *
 * Function         LPM_Enable
 *
 * Description      This function is called to register LPM with stack
 *
 * Parameter        p_cb: callback functions.
 *
 * Returns          void
 *
 ******************************************************************************/
void LPM_Enable(tLPM_CBACK* p_cb) {
  LOG_DEBUG(LOG_TAG, "%s:", __func__);
  lpm_cb.lpm_cback = *p_cb;
  if (srvc_attr_list.size() != 0) {
    lpm_cb.lpm_cback.p_gatts_service_cb(srvc_attr_list);
  }
}

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
void LPM_ModeSwitchReq(tLPM_mode mode) {
  LOG_DEBUG(LOG_TAG, "%s:", __func__);
  lpm_cb.mode = mode;
  LOG_DEBUG(LOG_TAG, "Mode: %d", mode);
  if (mode == LPM_TRACKER) {
    lpm_cb.refresh_raddr_timer_state = btm_ble_cancel_refresh_raddr_timer();
  } else if ((mode == LPM_ACTIVE) && lpm_cb.refresh_raddr_timer_state) {
    btm_ble_restart_refresh_raddr_timer();
    lpm_cb.refresh_raddr_timer_state = false;
  }
}

/*******************************************************************************
 *
 * Function         btu_inform_lpm_profile
 *
 * Description      Informs about Tx packet at hci layer
 *
 * Parameter        None
 *
 * Returns          None
 *
 ******************************************************************************/
void btu_inform_lpm_profile(void) {
  lpm_cb.lpm_cback.p_packet_after_flow_off();
}

/*******************************************************************************
 *
 * Function         bta_rmt_srvc_disc_done
 *
 * Description      Informs LPM that remote service discovery is done
 *
 * Parameter        hci handle, service list
 *
 * Returns          None
 *
 ******************************************************************************/
void bta_rmt_srvc_disc_done(uint16_t hci_handle, btgatt_db_element_t* db, uint8_t count) {
  LOG_DEBUG(LOG_TAG, "%s:", __func__);
  lpm_cb.lpm_cback.p_gatts_rmt_srvc_db_cb(hci_handle, db, count);
}

/*******************************************************************************
 *
 * Function         lpm_send_conn_cback
 *
 * Description      Informs about ACL connection to LPM higher layers
 *
 * Parameter        bda: Remote address
 *                  conn: Connection Role (Master/Slave/Disconnected)
 *
 * Returns          None
 *
 ******************************************************************************/
void lpm_send_conn_cback(const RawAddress& bda, int state) {
  LOG_DEBUG(LOG_TAG, "%s:", __func__);
  if (state == LPM_BLE_NONE) {
    lpm_cb.lpm_cback.p_ble_conn_cb(bda, 0, 0, 0, 0);
    auto it = lpm_cb.ble_conn.begin();
    for(;it != lpm_cb.ble_conn.end(); it++) {
      if(it->rmt_addr == bda)
        break;
    }
    lpm_cb.ble_conn.erase(it);
  } else {
    tACL_CONN* p = btm_bda_to_acl(bda, BT_TRANSPORT_LE);
    lpm_cb.ble_conn.emplace_back();
    tLPM_BLE_CONN& conn = lpm_cb.ble_conn.back();
    conn.master = state;
    conn.rmt_addr = bda;
    conn.ble_handle = p->hci_handle;
    lpm_cb.lpm_cback.p_ble_conn_cb(bda, 1, ((state == LPM_BLE_MASTER)? 1: 0),1, conn.ble_handle );
    /* tcb is skipped as of now */
  }
}


/*******************************************************************************
 *
 * Function         lpm_gatts_serv_db_element
 *
 * Description      Updates gatts db elements
 *
 * Parameter        db_el: GATTS DB
 *
 * Returns          None
 *
 ******************************************************************************/
void lpm_gatts_serv_db_element(tGATT_SRV_LIST_ELEM& db_el) {
  tGATT_SVC_DB* p_srvc_db = NULL;
  Uuid svc_uuid;
  Uuid char_uuid;
  tGATT_VALUE gap_attr_value;

  LOG_DEBUG(LOG_TAG, "%s:", __func__);
  if (db_el.type == GATT_UUID_PRI_SERVICE) {
    p_srvc_db = db_el.p_db;
  srvc_attr_list.clear();
  }

  if (p_srvc_db || !(p_srvc_db->attr_list.empty())) {
    svc_uuid = p_srvc_db->attr_list[0].p_value->uuid;
  if (svc_uuid == Uuid::From16Bit(UUID_SERVCLASS_GATT_SERVER)) {
      srvc_attr_list.emplace_back();
      tLPM_GATTS_SRVC_ATTR& srvc_attr = srvc_attr_list.back();
      srvc_attr.attr_uuid = Uuid::From16Bit(UUID_SERVCLASS_GATT_SERVER);
      srvc_attr.handle = db_el.s_hdl;
      srvc_attr.gatt_type = BTGATT_DB_PRIMARY_SERVICE;
      for (auto& attr : p_srvc_db->attr_list) {
        if (attr.uuid == Uuid::From16Bit(GATT_UUID_GATT_SRV_CHGD)) {
        srvc_attr_list.emplace_back();
        srvc_attr = srvc_attr_list.back();
        srvc_attr.attr_uuid = Uuid::From16Bit(GATT_UUID_GATT_SRV_CHGD);
        srvc_attr.handle = attr.handle;
        srvc_attr.gatt_type = BTGATT_DB_CHARACTERISTIC;
        break;
        }
      }
      /*GATTS is usually intialized before LPM is registered*/
      /*Callback is sent after LPM is registered*/
  } else if (svc_uuid == Uuid::From16Bit(UUID_SERVCLASS_GAP_SERVER)) {
      srvc_attr_list.emplace_back();
      tLPM_GATTS_SRVC_ATTR& srvc_attr = srvc_attr_list.back();
      srvc_attr.attr_uuid = Uuid::From16Bit(UUID_SERVCLASS_GAP_SERVER);
      srvc_attr.handle = db_el.s_hdl;
      srvc_attr.gatt_type = BTGATT_DB_PRIMARY_SERVICE;
      for (auto& attr : p_srvc_db->attr_list) {
        char_uuid = attr.uuid;
        if (char_uuid == Uuid::From16Bit(GATT_UUID_GAP_DEVICE_NAME) ||
                 char_uuid == Uuid::From16Bit(GATT_UUID_GAP_ICON) ||
                 char_uuid== Uuid::From16Bit(GATT_UUID_GAP_CENTRAL_ADDR_RESOL)) {
          if (read_gap_attr_value(attr.handle, &gap_attr_value, 0) == GATT_SUCCESS) {
            srvc_attr_list.emplace_back();
            srvc_attr = srvc_attr_list.back();
            srvc_attr.attr_uuid = attr.uuid;
            srvc_attr.handle = gap_attr_value.handle;
            srvc_attr.gatt_type = BTGATT_DB_CHARACTERISTIC;
            srvc_attr.offset = gap_attr_value.offset;
            strlcpy((char *)srvc_attr.value, (char *)gap_attr_value.value, gap_attr_value.len);
          }
        }
      }
      /*GATTS is usually intialized before LPM is registered*/
      /*Callback is sent after LPM is registered*/
    }
  }
}

void LPM_CopyPortInfo(tPORT* port) {
  LOG_DEBUG(LOG_TAG, "%s:  port->dlci = %d, port->scn = %d", __func__,port->dlci, port->scn);
  tLPM_PORT_INFO p_lpm_port_info;
  p_lpm_port_info.state = port->state;
  p_lpm_port_info.scn = port->scn;
  p_lpm_port_info.uuid = port->uuid;
  p_lpm_port_info.bd_addr = port->bd_addr;
  p_lpm_port_info.is_server = port->is_server;
  p_lpm_port_info.dlci = port->dlci;
  p_lpm_port_info.mtu = port->mtu;
  p_lpm_port_info.peer_mtu = port->peer_mtu;
  p_lpm_port_info.credit_tx = port->credit_tx;
  p_lpm_port_info.credit_rx = port->credit_rx;
  p_lpm_port_info.credit_rx_max = port->credit_rx_max;
  p_lpm_port_info.credit_rx_low = port->credit_rx_low;
  p_lpm_port_info.keep_mtu = port->keep_mtu;
  //send callback to bta
  LOG_DEBUG(LOG_TAG, "%s:  sendong to bta layer", __func__);
  lpm_cb.lpm_cback.p_rfcomm_port_info_cb(&p_lpm_port_info);
}

void LPM_ReleasePortInfo(tPORT* port) {
  LOG_DEBUG(LOG_TAG, "%s:  port->dlci = %d", __func__,port->dlci);
  tLPM_PORT_INFO p_lpm_port_info;
  p_lpm_port_info.state = port->state;
  p_lpm_port_info.scn = port->scn;
  p_lpm_port_info.uuid = port->uuid;
  p_lpm_port_info.bd_addr = port->bd_addr;
  p_lpm_port_info.is_server = port->is_server;
  p_lpm_port_info.dlci = port->dlci;
  p_lpm_port_info.mtu = port->mtu;
  p_lpm_port_info.peer_mtu = port->peer_mtu;
  p_lpm_port_info.credit_tx = port->credit_tx;
  p_lpm_port_info.credit_rx = port->credit_rx;
  p_lpm_port_info.credit_rx_max = port->credit_rx_max;
  p_lpm_port_info.credit_rx_low = port->credit_rx_low;
  p_lpm_port_info.keep_mtu = port->keep_mtu;
  //send callback to bta
  LOG_DEBUG(LOG_TAG, "%s:  sendong to bta layer", __func__);
  lpm_cb.lpm_cback.p_rfcomm_port_release_cb(&p_lpm_port_info);

}

void LPM_CopyHCIInfo(uint16_t handle,uint8_t role,const RawAddress& bda){
   LOG_DEBUG(LOG_TAG, "%s", __func__);
   //need to populate structure here
   tLPM_HCI_INFO p_lpm_hci_info;
   std::string addr = bda.ToString();
   LOG_DEBUG(LOG_TAG, "%s bda = %s", __func__,bda.ToString().c_str());
   p_lpm_hci_info.hci_handle = handle;
   p_lpm_hci_info.role = role;
   p_lpm_hci_info.mode =0;
   p_lpm_hci_info.direction = 1;
   lpm_cb.lpm_cback.p_hci_info_cb(&p_lpm_hci_info,bda);
}

void LPM_CopyL2CAPCredits(tL2CAP_credits credits) {
   LOG_DEBUG(LOG_TAG, "%s", __func__);
   lpm_cb.lpm_cback.P_l2cap_credit_info_cb(credits);
}

void LPM_CopyL2CAPInfo(const RawAddress& bda, uint16_t psm, uint16_t local_cid, uint16_t remote_cid, uint16_t local_mtu, uint16_t remote_mtu) {
   LOG_DEBUG(LOG_TAG, "%s", __func__);
   tLPM_L2CAP_INFO p_lpm_l2cap_info;
   p_lpm_l2cap_info.psm = psm;
   p_lpm_l2cap_info.local_cid = local_cid;
   p_lpm_l2cap_info.remote_cid = remote_cid;
   p_lpm_l2cap_info.local_mtu = local_mtu;
   p_lpm_l2cap_info.remote_mtu = remote_mtu;
   lpm_cb.lpm_cback.p_l2cap_info_cb(&p_lpm_l2cap_info, bda);
}

void LPM_CopyRfCommCredits(uint8_t dlci, uint16_t credits, bool is_tx_credits) {
   LOG_DEBUG(LOG_TAG, "%s, dlci %d credits %d, is tx %d", __func__, dlci, credits, is_tx_credits);
   tLPM_RFCOMM_CREDITS p_lpm_rfcomm_credit;
   p_lpm_rfcomm_credit.dlci = dlci;
   p_lpm_rfcomm_credit.credits = credits;
   p_lpm_rfcomm_credit.is_tx_credits = is_tx_credits;
   lpm_cb.lpm_cback.p_rfcomm_credit_info_cb(&p_lpm_rfcomm_credit);
}
