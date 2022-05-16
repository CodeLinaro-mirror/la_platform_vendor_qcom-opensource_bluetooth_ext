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

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "btif_lpm"
#endif

/*******************************************************************************
 *  Structures and Macros
 ******************************************************************************/


/*******************************************************************************
 *  Static variables
 ******************************************************************************/
static btif_lpm_cb_t btif_lpm_cb;

void btif_lpm_evt(tBTA_LPM_EVT event, tBTA_LPM* p_data);

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

  /* Post LPM init to the state machine */

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

  /* Deinitialize LPM state machine */

  BTA_DisableLPM();
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
  /* Call LPM Native interface function */
  }

  BTIF_STORAGE_FILL_PROPERTY(&prop_name, BT_PROPERTY_BDNAME,
                            sizeof(bt_bdname_t), &bdname);

  if (btif_storage_get_adapter_property(&prop_name) == BT_STATUS_SUCCESS) {
    strlcpy(btif_lpm_cb.device_name, (char*) bdname.name, BD_NAME_LEN + 1);
  /* Call LPM Native interface function */
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
    default:
      break;
  }
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