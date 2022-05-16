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

/* BTIF LPM Control Block */
typedef struct {
  bool privacy_enabled;
  std::string device_addr;
  char device_name[BD_NAME_LEN + 1];
  uint8_t logging_byte;
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

#endif /* BTIF_LPM_H */
