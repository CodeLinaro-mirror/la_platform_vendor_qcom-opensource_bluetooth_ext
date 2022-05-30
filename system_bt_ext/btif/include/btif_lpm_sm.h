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

#ifndef BTIF_LPM_SM_H
#define BTIF_LPM_SM_H

#include "btif_sm.h"
#include <base/bind.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include "osi/include/wakelock.h"
#include <string.h>
#include <map>

#include <hardware/bt_rc.h>

/*******************************************************************************
 * BTIF LPM STATE MACHINE Enums
 ******************************************************************************/

/* Btif LPM States */
typedef enum {
  BTIF_LPM_STATE_STACK_IDLE = 0,
  BTIF_LPM_STATE_STACK_TWM_PENDING,
  BTIF_LPM_STATE_STACK_ACTIVE,
  BTIF_LPM_STATE_STACK_ACTIVE_PENDING,
  BTIF_LPM_STATE_STACK_OFFLOAD_PENDING,
  BTIF_LPM_STATE_STACK_OFFLOADED,
} btif_lpm_state_t;

/* Btif LPM commands and events */
typedef enum {
  BTIF_LPM_SM_INIT_LPM_PROFILE = 1,
  BTIF_LPM_SM_REINIT_LPM_NATIVE,
  BTIF_LPM_SM_SEND_LPM_ENTER_CMD,
  BTIF_LPM_SM_SEND_LPM_EXIT_CMD,
  BTIF_LPM_SM_CHECK_OFFLOAD_CMD,
  BTIF_LPM_SM_SEND_FLOW_ON_CMD,
  BTIF_LPM_SM_SEND_FLOW_OFF_CMD,
  BTIF_LPM_SM_LPM_ENTER_ACK_EVT,
  BTIF_LPM_SM_CHECK_OFFLOAD_ACK_EVT,
  BTIF_LPM_SM_LPM_EXIT_EVT,
  BTIF_LPM_SM_FLOW_ON_EVT,
  BTIF_LPM_SM_FLOW_OFF_EVT,
  BTIF_LPM_SM_TIMER_ACTIVE_EVT,
  BTIF_LPM_SM_TIMER_INACTIVE_EVT,
  BTIF_LPM_SM_PROFILE_UPDATE_EVT,
  BTIF_LPM_SM_TWM_OFFLOAD_CONT_EVT,
  BTIF_LPM_SM_NOTIFY_NACK_MSG,
} btif_lpm_sm_event_t;

typedef enum {
    BTIF_SM_SEND_FLOW_OFF_CMD = 0,
    BTIF_SM_SEND_FLOW_ON_CMD,
    BTIF_SM_SEND_LPM_ENTER_CMD,
    BTIF_SM_SEND_LPM_EXIT_CMD,
} btif_lpm_event_t;

/* Btif LPM Native Status */
enum LPMNativeStatus : int32_t {
  LPM_SUCCESS,
  LPM_FAILED,
  LPM_PENDING,
};

/* Btif Stack Status */
enum StackStatus : int32_t {
  OFFLOAD_SUCCESS,
  OFFLOAD_FAILED,
  OFFLOAD_PENDING,
};

/* Btif BT Status */
enum BTStatus : int32_t {
  BT_ACTIVE,
  BT_INACTIVE,
};

/*****************************************************************************
 *  BTIF LPM SM Function Declarations
 ****************************************************************************/

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
void btif_dispatch_lpm_event(btif_lpm_sm_event_t event, void *p_data, int len);

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
void initialize_lpm_sm(void);

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
void deinitialize_lpm_sm(void);

/*******************************************************************************
 *
 * Function         btif_lpm_sm_get_state
 *
 * Description      To get current state of LPM SM
 *
 * Parameters       None
 *
 * Returns          btif_sm_state_t
 *
 ******************************************************************************/
btif_lpm_state_t btif_lpm_sm_get_state(void);

/*******************************************************************************
 *
 * Function         btif_lpm_sm_set_discovery_cancel
 *
 * Description      To set weather we initiated stop discovery or not
 *
 * Parameters       true if we started cancel discovery
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_sm_set_discovery_cancel(bool value);

#endif /* BTIF_LPM_SM_H */ 
