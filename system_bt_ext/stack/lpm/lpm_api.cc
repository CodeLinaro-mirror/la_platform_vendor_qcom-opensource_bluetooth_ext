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
  std::vector<tLPM_BLE_CONN> ble_conn;
} tLPM_CB;

/* Control Block */
tLPM_CB lpm_cb;

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
void lpm_send_conn_cback(const RawAddress& bda, int conn) {
  LOG_DEBUG(LOG_TAG, "%s:", __func__);
  if (master == LPM_BLE_NONE) {
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
    conn.master = master;
    conn.rmt_addr = bda;
    conn.ble_handle = p->hci_handle;
    lpm_cb.lpm_cback.p_ble_conn_cb(bda, 1, ((master == LPM_BLE_MASTER)? 1: 0),1, conn.ble_handle );
    /* tcb is skipped as of now */
  }
}
