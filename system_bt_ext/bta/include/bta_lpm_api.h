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
 *  This is the public interface file for BTA LPM.
 *
 ******************************************************************************/

#ifndef BTA_LPM_API_H
#define BTA_LPM_API_H

#include "bta_sys.h"
#include "bta_api.h"
#include "lpm_api.h"
#include "l2c_api.h"

/*****************************************************************************
 *  LPM Profile Callback data definitions from BTA to BTIF
 ****************************************************************************/

/* Callback events from BTA to BTIF */
#define LPM_PROFILE_UPDATE_EVT 0
#define LPM_BLE_CONN_EVT 1
#define LPM_GATTS_EVT 2
#define LPM_ALARM_EVT 3
#define LPM_PACKET_AFTER_FLOW_OFF 4
#define LPM_RFCOMM_EVT 7
#define LPM_HCI_INFO_EVT 8
#define LPM_L2CAP_EVT 9

typedef uint8_t tBTA_LPM_EVT;

/* LPM Profile Data structures for events from BTA to BTIF */
typedef enum {
    BTA_LPM_CONN_PROGRESS,
    BTA_LPM_CONN_DONE,
    BT_LPM_SCO_ACTIVE,
    BT_LPM_SCO_INACTIVE,
    BTA_LPM_DISC_PROGRESS,
    BTA_LPM_DISC_DONE
} tBTA_LPM_PROFILE_UPDATE_EVT;

typedef enum {
    BTA_LPM_SOCKET_CONN_OPEN,
    BTA_LPM_SOCKET_CONN_CLOSED
} tBTA_LPM_RFCOMM_CONN_EVT;

typedef enum {
  BTA_LPM_BREDR_CONN_OPEN,
  BTA_LPM_BREDR_CONN_CLOSED
}tBTA_LPM_HCI_CONN_EVT;

typedef struct {
  RawAddress rmt_addr;
  bool rmt_addr_type;
  uint16_t hci_handle;
  bool conn_evt;
  bool master;
} tBTA_LPM_BLE_CONN_UPDATE;

typedef struct {
  uint8_t num_attr;
  tLPM_GATTS_SRVC_ATTR* srvc_attr;
} tBTA_LPM_GATTS_EVT;

enum {
 ALARM_START,
 ALARM_STOP
};

typedef struct {
  char name[50];
  uint8_t state;
}tBTA_LPM_ALARM_EVT;

typedef struct {
  uint8_t profile_id; /* BTA subsystem id */
  tBTA_LPM_PROFILE_UPDATE_EVT profile_evt;
} tBTA_LPM_PROFILE_UPDATE;

typedef struct {
    tBTA_LPM_RFCOMM_CONN_EVT conn_evt;
    tLPM_PORT_INFO port_info;
} tBTA_LPM_RFCOMM_EVT;

typedef struct {
  tBTA_LPM_HCI_CONN_EVT conn_evt;
  tLPM_HCI_INFO hci_info;
  //std::string bd_addr;
  RawAddress bd_addr;
} tBTA_LPM_HCI_EVT;

typedef struct {
  tL2CAP_credits credit_info;
}tBTA_LPM_L2CAP_EVT;

typedef union {
  tBTA_LPM_PROFILE_UPDATE profile_update;
  tBTA_LPM_BLE_CONN_UPDATE ble_conn;
  tBTA_LPM_GATTS_EVT gatts_srvc;
  tBTA_LPM_ALARM_EVT alarm_evt;
  tBTA_LPM_RFCOMM_EVT rfcomm_evt;
  tBTA_LPM_HCI_EVT hci_evt;
  tBTA_LPM_L2CAP_EVT l2cap_evt;
} tBTA_LPM;

/*  LPM Profile Callback from BTA to BTIF */
typedef void(tBTA_LPM_CBACK)(tBTA_LPM_EVT event, tBTA_LPM* p_data);

/*****************************************************************************
 *  BTA APIs for LPM Profile
 ****************************************************************************/

/*******************************************************************************
 *
 * Function         BTA_EnableLPM
 *
 * Description      API to Enable BTA LPM.
 *
 * Parameters       p_cback - Callback to btif
 *
 * Returns          tBTA_STATUS
 *
 ******************************************************************************/
extern tBTA_STATUS BTA_EnableLPM(tBTA_LPM_CBACK* p_cback);

/*******************************************************************************
 *
 * Function         BTA_DisableLPM
 *
 * Description      API to clean-up/disable BTA.
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
extern void BTA_DisableLPM();

/*******************************************************************************
 *
 * Function         BTA_EnableLPM
 *
 * Description      API to Switch mode of BTA LPM.
 *
 * Parameters       mode - Desired mode of the platform
 *
 * Returns          tBTA_STATUS
 *
 ******************************************************************************/
extern tBTA_STATUS BTA_LPM_ModeSwitchReq(tLPM_mode mode);

#endif /* BTA_LPM_API_H */

