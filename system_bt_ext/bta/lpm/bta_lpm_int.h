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
 *  This is the private interface file for the BTA LPM
 *
 ******************************************************************************/

#ifndef BTA_LPM_INT_H
#define BTA_LPM_INT_H

#include "bta_lpm_api.h"

/*****************************************************************************
 *  Structures and Constants
 ****************************************************************************/
enum {
  BTA_MODE_SWITCH_REQ = BTA_SYS_EVT_START(BTA_ID_LPM),
};

/* BTA LPM Control Block */
typedef struct {
  bool is_bta_lpm_active;
  tBTA_LPM_CBACK* p_lpm_cback;
} tBTA_LPM_CB;

typedef struct {
  BT_HDR hdr;
  tLPM_mode mode;
} tBTA_LPM_MODE_SWITCH;

typedef struct {
  BT_HDR hdr;
  tBTA_LPM_MODE_SWITCH mode_switch;
} tBTA_LPM_MSG;

/*****************************************************************************
 *  BTA LPM Internal Function Declarations
 ****************************************************************************/

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
extern bool bta_lpm_hdl_msg(BT_HDR* p_msg);

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
extern void lpm_profile_update_evt(uint8_t profile, tBTA_LPM_PROFILE_UPDATE_EVT event);

#endif /* BTA_LPM_INT_H */
