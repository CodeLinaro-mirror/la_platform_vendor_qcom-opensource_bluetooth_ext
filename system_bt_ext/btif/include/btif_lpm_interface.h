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
 *  Filename:      btif_lpm_interface.h
 *
 *  Description:   Header file for all interface apis of LPM native
 *
 ******************************************************************************/

#ifndef BTIF_LPM_INTERFACE_H
#define BTIF_LPM_INTERFACE_H

#include "btif_sm.h"
#include "btif_lpm_sm.h"
#include "btif_lpm.h"
#include <vendor/qti/hardware/bluetooth_offload/1.0/IBluetoothOffload.h>
#include <vendor/qti/hardware/bluetooth_offload/1.0/IBluetoothOffloadCb.h>
#include <vendor/qti/hardware/bluetooth_offload/1.0/types.h>
#include <android/hidl/base/1.0/IBase.h>
#include <hidl/HidlSupport.h>
#include <utils/Log.h>

namespace vendor {
namespace qti {
namespace hardware {
namespace bluetooth_offload {
namespace V1_0 {

using namespace android;
using namespace android::hardware;
using namespace ::vendor::qti::hardware::bluetooth_offload::V1_0;

/*****************************************************************************
 *  BTIF LPM Native Function Declarations
 ****************************************************************************/

/*******************************************************************************
 *
 * Function         btif_lpm_native_interface_init
 *
 * Description      Initializes LPM Native interface
 *
 * Parameters       None
 *
 * Returns          Status
 *
 ******************************************************************************/
bool btif_lpm_native_interface_init(void);

/*******************************************************************************
 *
 * Function         btif_lpm_native_interface_deinit
 *
 * Description      De-Initializes LPM Native interface
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_native_interface_deinit(void);

/*******************************************************************************
 *
 * Function         btif_lpm_native_flow_on_ack_interface
 *
 * Description      Inform LPM Native interface that Flow On is done
 *
 * Parameters       Status to be sent to LPM Native
 *
 * Returns          Success or Failure of the API
 *
 ******************************************************************************/
bool btif_lpm_native_flow_on_ack_interface(uint8_t status);

/*******************************************************************************
 *
 * Function         btif_lpm_native_flow_off_ack_interface
 *
 * Description      Inform LPM Native interface that Flow Off is done
 *
 * Parameters       Status to be sent to LPM Native
 *
 * Returns          Success or Failure of the API
 *
 ******************************************************************************/
bool btif_lpm_native_flow_off_ack_interface(uint8_t status);

/*******************************************************************************
 *
 * Function         btif_lpm_native_disable_offload_status_interface
 *
 * Description      Inform LPM Native interface that Disable Offload is done
 *
 * Parameters       Status to be sent to LPM Native
 *
 * Returns          Success or Failure of the API
 *
 ******************************************************************************/
bool btif_lpm_native_disable_offload_status_interface(LPMNativeStatus status);

/*******************************************************************************
 *
 * Function         btif_lpm_native_interface_prepare_offload_done
 *
 * Description      Inform LPM Native interface that Prepare Offload is done
 *
 * Parameters       lpm_status - status to be sent to LPM Native
 *                  stack_status - offload status
 *
 * Returns          Success or Failure of the API
 *
 ******************************************************************************/
bool btif_lpm_native_interface_prepare_offload_done(LPMNativeStatus lpm_status,
     StackStatus stack_status);

/*******************************************************************************
 *
 * Function         btif_lpm_native_interface_commit_offload_done
 *
 * Description      Inform LPM Native interface that Commit Offload is done
 *
 * Parameters       lpm_status - status to be sent to LPM Native
 *                  stack_status - offload status
 *
 * Returns          Success or Failure of the API
 *
 ******************************************************************************/
bool btif_lpm_native_interface_commit_offload_done(
    LPMNativeStatus lpm_status, StackStatus stack_status);

/*******************************************************************************
 *
 * Function         btif_lpm_native_bt_activity_status_interface
 *
 * Description      Wrapper to get BT Activity Status to LPM Native interface
 *
 * Parameters       None
 *
 * Returns          Offload status
 *
 ******************************************************************************/
StackStatus btif_lpm_native_bt_activity_status_interface(void);

/*******************************************************************************
 *
 * Function         btif_lpm_native_interface_send_le_hci_context_info
 *
 * Description      Wrapper to send LE HCI Context information to LPM Native interface
 *
 * Parameters       addr - Address of remote device
 *                  LeHciContext - Context info to be sent to LPM Native
 *
 * Returns          Success or Failure of the API
 *
 ******************************************************************************/
bool btif_lpm_native_interface_send_le_hci_context_info(
    std::string addr, HciLeContextType LeHciContext);

/*******************************************************************************
 *
 * Function         btif_lpm_native_interface_send_le_gatt_context_info
 *
 * Description      Wrapper to send LE Gatt Context information to LPM Native interface
 *
 * Parameters       addr - Address of remote device
 *                  gattContext - Context info to be sent to LPM Native
 *
 * Returns          Success or Failure of the API
 *
 ******************************************************************************/
bool btif_lpm_native_interface_send_le_gatt_context_info(
    std::string remoteAddr, GattContextType gattContext);

/*******************************************************************************
 *
 * Function         btif_lpm_native_interface_send_le_gatt_handle_range_info
 *
 * Description      Wrapper to send LE Gatt Handle Range to LPM Native interface
 *
 * Parameters       addr - Address of remote device
 *                  serviceInfo - Gatt Service Info to be sent to LPM Native
 *
 * Returns          Success or Failure of the API
 *
 ******************************************************************************/
bool btif_lpm_native_interface_send_le_gatt_handle_range_info(
    std::string remoteAddr, std::vector<GattServiceInfo> serviceInfo);

/*******************************************************************************
 *
 * Function         btif_lpm_native_set_local_device_name
 *
 * Description      Wrapper to set local device name to LPM Native interface
 *
 * Parameters       name - local name to be set
 *
 * Returns          Success or Failure of the API
 *
 ******************************************************************************/
bool btif_lpm_native_set_local_device_name(std::string name);

/*******************************************************************************
 *
 * Function         btif_lpm_native_set_local_device_address
 *
 * Description      Wrapper to set local device address to LPM Native interface
 *
 * Parameters       addr - local device addr to be set
 *                  privacy - to know the address type
 *
 * Returns          Success or Failure of the API
 *
 ******************************************************************************/
bool btif_lpm_native_set_local_device_address(std::string addr, bool privacy);

/*******************************************************************************
 *
 * Function         btif_lpm_native_set_le_context_info
 *
 * Description      Wrapper to set LE Context Information to LPM Native interface
 *
 * Parameters       ble_conn - BLE Connection structure
 *                  dev_addr - local device address
 *
 * Returns          Success or Failure of the API
 *
 ******************************************************************************/
bool btif_lpm_native_set_le_context_info(btif_lpm_ble_conn_t& ble_conn, std::string dev_addr);
bool btif_lpm_native_interface_set_rfcomm_context_info(std::vector<btif_rfcomm_context_t>,std::string dev_addr);
}  // namespace V1_0
}  // namespace bluetooth_offload
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
#endif /* BTIF_LPM_INTERFACE_H  */
