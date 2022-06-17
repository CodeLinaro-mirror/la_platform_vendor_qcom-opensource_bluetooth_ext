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
 *  Filename:      lpm_int.h
 *
 *  Description:   Private stack interface file for LPM Profile
 *
 ******************************************************************************/

#ifndef LPM_INT_H
#define LPM_INT_H

#include "bt_types.h"
#include "lpm_api.h"
#include "stack/gatt/gatt_int.h"

/*****************************************************************************
 *  Structures and Macros
 ****************************************************************************/

#define LPM_BLE_NONE 0x00
#define LPM_BLE_SLAVE 0x01
#define LPM_BLE_MASTER 0x02

/*****************************************************************************
 * LPM Internal Function Declarations
 ****************************************************************************/

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
extern void lpm_send_conn_cback(const RawAddress& bda, int master);

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
extern void lpm_gatts_serv_db_element(tGATT_SRV_LIST_ELEM& db_el);

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
extern void btu_inform_lpm_profile();
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
extern void bta_rmt_srvc_disc_done(uint16_t hci_handle, btgatt_db_element_t* db, uint8_t count);
#endif /* LPM_INT_H */
