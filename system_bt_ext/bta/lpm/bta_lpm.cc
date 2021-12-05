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
 *  This is the implementation file of BTA LPM
 *
 ******************************************************************************/

#include "bta_lpm_api.h"
#include "bta_lpm_int.h"

#include <base/bind_helpers.h>
#include "string.h"

/*****************************************************************************
 *  Static and Constants
 ****************************************************************************/
tBTA_LPM_CB bta_lpm_cb;

static const tBTA_SYS_REG bta_lpm_reg = {bta_lpm_hdl_msg, BTA_DisableLPM};

void bta_lpm_enable(tBTA_LPM_CBACK* p_cback);

static void alarm_info_cb (bool state, char* name);

static void bta_lpm_ble_conn_cb (const RawAddress& rmt_addr, bool addr_type, bool master,
                                            bool connected, uint16_t hci_handle);

static void bta_lpm_gatts_service_cb (std::vector<tLPM_GATTS_SRVC_ATTR> attr_list);

static void bta_lpm_packet_after_flow_off_cb(void);

static void bta_lpm_rfcomm_port_cb(tLPM_PORT_INFO*);

static void bta_lpm_hci_info_cb(tLPM_HCI_INFO* info, const RawAddress& bd_addr);

static tLPM_CBACK bta_lpm_cback = {bta_lpm_ble_conn_cb,
                                   bta_lpm_gatts_service_cb,
                                   bta_lpm_packet_after_flow_off_cb,
                                   bta_lpm_rfcomm_port_cb,
                                   bta_lpm_hci_info_cb
                                  };

#ifndef CASE_RETURN_STR
#define CASE_RETURN_STR(const) \
  case const:                  \
    return #const;
#endif

const char* dump_lpm_event(uint16_t event) {
  switch (event) {
    CASE_RETURN_STR(BTA_MODE_SWITCH_REQ)
    default:
      return "UNKNOWN_EVENT";
  }
}

/*******************************************************************************
 *
 * Function         BTA_LPMDisable
 *
 * Description      This function is called to disable LPM module
 *
 * Parameters       None.
 *
 * Returns          None
 *
 ******************************************************************************/
void BTA_DisableLPM(void) {
  if (bta_lpm_cb.is_bta_lpm_active || !bta_sys_is_register(BTA_ID_LPM)) {
      return;
  }

  alarm_deregister_info();
  bta_sys_deregister(BTA_ID_LPM);
}

/*******************************************************************************
 *
 * Function         BTA_EnableLPM
 *
 * Description      Registers LPM Profile with BTA sys interface.
 *
 * Parameters       p_cback - Callback to btif
 *
 * Returns          tBTA_STATUS
 *
 ******************************************************************************/
tBTA_STATUS BTA_EnableLPM(tBTA_LPM_CBACK* p_cback) {
  if (p_cback == NULL || bta_lpm_cb.is_bta_lpm_active) {
    return BTA_FAILURE;
  }

  /* register with BTA system manager */
  bta_sys_register(BTA_ID_LPM, &bta_lpm_reg);

  do_in_bta_thread(FROM_HERE, base::Bind(&bta_lpm_enable, p_cback));
  return BTA_SUCCESS;
}

/*******************************************************************************
 *
 * Function         BTA_ModeSwitchReq
 *
 * Description      API to switch mode of LPM profile.
 *
 * Parameters       mode - Next mode of platform
 *
 * Returns          tBTA_STATUS
 *
 ******************************************************************************/
tBTA_STATUS BTA_LPM_ModeSwitchReq(tLPM_mode mode) {
  if (bta_lpm_cb.is_bta_lpm_active) {
    return BTA_FAILURE;
  }

  tBTA_LPM_MODE_SWITCH* p_buf =
    (tBTA_LPM_MODE_SWITCH*)osi_malloc(sizeof(tBTA_LPM_MODE_SWITCH));
  p_buf->hdr.event = BTA_MODE_SWITCH_REQ;
  p_buf->mode = mode;
  bta_sys_sendmsg(p_buf);

  return BTA_SUCCESS;
}

/*******************************************************************************
 *
 * Function         bta_lpm_enable
 *
 * Description      Registers btif callback for BTA LPM
 *                  Enables LPM Profile in stack
 *
 * Parameters       p_cback - Callback to btif
 *
 * Returns          void
 *
 ******************************************************************************/
void bta_lpm_enable(tBTA_LPM_CBACK* p_cback) {
  bta_lpm_cb.p_lpm_cback = p_cback;
  LPM_Enable(&bta_lpm_cback);
  bta_lpm_cb.is_bta_lpm_active = true;
  alarm_register_info(alarm_info_cb);
}

/*******************************************************************************
 *
 * Function         bta_lpm_mode_switch
 *
 * Description      Sends mode switch mode to stack
 *
 * Parameters       p_data - Data from higher layer
 *
 * Returns          void
 *
 ******************************************************************************/
void bta_lpm_mode_switch(tBTA_LPM_MSG* p_data) {
  LPM_ModeSwitchReq(p_data->mode_switch.mode);
}

/*******************************************************************************
 *
 * Function         bta_lpm_hdl_msg
 *
 * Description      LPM main event handling function.
 *
 * Parameters       p_msg - Data related to event
 *
 * Returns          bool
 *
 ******************************************************************************/
bool bta_lpm_hdl_msg(BT_HDR* p_msg) {
  APPL_TRACE_DEBUG(" %s event = %s", __func__, dump_lpm_event(p_msg->event));
  bool ret = true;
  switch(p_msg->event) {
  case BTA_MODE_SWITCH_REQ:
    bta_lpm_mode_switch((tBTA_LPM_MSG*)p_msg);
    break;
  default:
       break;
  }
  return ret;
}

/*******************************************************************************
 *
 * Function         lpm_profile_update_evt
 *
 * Description      Informs btif state machine regarding profile update
 *
 * Parameters       profile - BTA sys profile id
 *                  event - state of profile
 *
 * Returns          void
 *
 ******************************************************************************/
void lpm_profile_update_evt(uint8_t profile, tBTA_LPM_PROFILE_UPDATE_EVT event) {
  APPL_TRACE_DEBUG(" %s profile: %d event: %d", __func__, profile, event);
  if(bta_lpm_cb.p_lpm_cback != NULL) {
    tBTA_LPM profile_update_evt;
    profile_update_evt.profile_update.profile_id = profile;
    profile_update_evt.profile_update.profile_evt= event;
    bta_lpm_cb.p_lpm_cback(LPM_PROFILE_UPDATE_EVT, (tBTA_LPM*)&profile_update_evt);
  }
}

/*******************************************************************************
 *
 * Function         bta_lpm_ble_conn_cb
 *
 * Description      LPM callback for services added
 *
 * Returns          void
 *
 ******************************************************************************/
static void bta_lpm_ble_conn_cb(const RawAddress& rmt_addr, bool addr_type,
                            bool master, bool connected, uint16_t hci_handle) {
  APPL_TRACE_DEBUG(" %s lpm connected: %d hci_handle %d master %d",
                                        __func__, connected, hci_handle, master);
  if(bta_lpm_cb.p_lpm_cback != NULL) {
    tBTA_LPM ble_conn_update;
    ble_conn_update.ble_conn.conn_evt = connected;
    ble_conn_update.ble_conn.rmt_addr = rmt_addr;
    ble_conn_update.ble_conn.rmt_addr_type = addr_type;
    ble_conn_update.ble_conn.master = master;
    ble_conn_update.ble_conn.hci_handle = hci_handle;
    bta_lpm_cb.p_lpm_cback(LPM_BLE_CONN_EVT, &ble_conn_update);
  }
}


/*******************************************************************************
 *
 * Function         bta_gatts_service_cb
 *
 * Description      LPM callback for services added
 *
 *feturns          void
 *
 ******************************************************************************/
static void bta_lpm_gatts_service_cb(std::vector<tLPM_GATTS_SRVC_ATTR> attr_list) {
  if(bta_lpm_cb.p_lpm_cback != NULL) {
    tBTA_LPM gatts_evt;
    gatts_evt.gatts_srvc.num_attr = attr_list.size();
    gatts_evt.gatts_srvc.srvc_attr = attr_list.data();
    bta_lpm_cb.p_lpm_cback(LPM_GATTS_EVT, &gatts_evt);
  }
}

/*******************************************************************************
 *
 * Function         bta_lpm_packet_after_flow_off_cb
 *
 * Description      LPM callback if Tx packet at hci/l2cap after flow off
 *
 *feturns          void
 *
 ******************************************************************************/
static void bta_lpm_packet_after_flow_off_cb(void) {
  APPL_TRACE_DEBUG("%s: ", __func__);
  if(bta_lpm_cb.p_lpm_cback != NULL) {
    bta_lpm_cb.p_lpm_cback(LPM_PACKET_AFTER_FLOW_OFF, NULL);
  }
}

/*******************************************************************************
 *
 * Function         alarm_info_cb
 *
 * Description      Callback for alarm info
 *
 * Returns          void
 *
 ******************************************************************************/
static void alarm_info_cb (bool state, char* name) {
  if(strcmp(name,"btm_ble_addr.refresh_raddr_timer") != 0) {
    tBTA_LPM alarm_evt;
    strlcpy(alarm_evt.alarm_evt.name,name,strlen(name));
    if (state)
      alarm_evt.alarm_evt.state = ALARM_START;
    else
      alarm_evt.alarm_evt.state = ALARM_STOP;
    if(bta_lpm_cb.p_lpm_cback != NULL)
      bta_lpm_cb.p_lpm_cback(LPM_ALARM_EVT, &alarm_evt);
  }
}


static void bta_lpm_rfcomm_port_cb (tLPM_PORT_INFO* port_info) {
  APPL_TRACE_DEBUG(" %s lpm callback ", __func__);
  if(bta_lpm_cb.p_lpm_cback != NULL) {
    tBTA_LPM rfcomm_context_evt;
    APPL_TRACE_DEBUG(" %s lpm callback ", __func__);
    rfcomm_context_evt.rfcomm_evt.conn_evt = BTA_LPM_SOCKET_CONN_OPEN;
    memset(&(rfcomm_context_evt.rfcomm_evt.port_info), 0, sizeof(rfcomm_context_evt.rfcomm_evt.port_info));
    if(port_info != NULL) {
      memcpy(&(rfcomm_context_evt.rfcomm_evt.port_info),port_info,sizeof(rfcomm_context_evt.rfcomm_evt.port_info));
      APPL_TRACE_ERROR(" rfcomm_context_evt.rfcomm_evt.port_info.dlci  = %d",rfcomm_context_evt.rfcomm_evt.port_info.dlci);
    }
   bta_lpm_cb.p_lpm_cback(LPM_RFCOMM_EVT, &rfcomm_context_evt);
  }
}

static void bta_lpm_hci_info_cb(tLPM_HCI_INFO* hci_info,const RawAddress& bd_addr){
  APPL_TRACE_DEBUG(" %s lpm callback addr: %s", __func__,bd_addr.ToString().c_str());
  tBTA_LPM hci_context_evt;
  if(hci_info != NULL){
    hci_context_evt.hci_evt.conn_evt = BTA_LPM_BREDR_CONN_OPEN;
    hci_context_evt.hci_evt.bd_addr = bd_addr;
    memset(&(hci_context_evt.hci_evt.hci_info), 0, sizeof(hci_context_evt.hci_evt.hci_info));
    memcpy(&(hci_context_evt.hci_evt.hci_info),hci_info,sizeof(hci_context_evt.hci_evt.hci_info));
    APPL_TRACE_DEBUG(" %s hci_info_copied handle: %d role %d", __func__,hci_context_evt.hci_evt.hci_info.hci_handle,hci_context_evt.hci_evt.hci_info.role);
  }
  bta_lpm_cb.p_lpm_cback(LPM_HCI_INFO_EVT, &hci_context_evt);
}
