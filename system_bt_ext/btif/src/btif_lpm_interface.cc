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
 *  Filename:      btif_lpm_interface.cc
 *
 *  Description:   Implements interface with LPM Native
 *
 ******************************************************************************/

#define LOG_TAG "btif_lpm_native_interface"

#include <cutils/properties.h>
#include <base/location.h>
#include <base/logging.h>
#include <vector>

#include "btif_lpm.h"
#include "btif_lpm_interface.h"
#include "osi/include/log.h"

using bluetooth::Uuid;

namespace vendor {
namespace qti {
namespace hardware {
namespace bluetooth_offload {
namespace V1_0 {

using namespace android;
using namespace android::hardware;
using ::android::sp;
using ::android::hardware::hidl_death_recipient;
using ::android::hidl::base::V1_0::IBase;
using namespace ::vendor::qti::hardware::bluetooth_offload::V1_0;

class LpmNativeDeathRecipient:public ::android::hardware::hidl_death_recipient {
public:
  virtual void serviceDied(uint64_t /*cookie*/,
         const android::wp<::android::hidl::base::V1_0::IBase>& /*who*/) {
    LOG_ERROR(LOG_TAG, "LPM Native HAL service died!");
    /* Post REINIT_NATIVE_HAL to state machine after a delay */
   }
};

LpmNativeDeathRecipient *lpmNativeDeathRecipient;
android::sp<IBluetoothOffload> btLpmNative;

/* Implementation of IBluetoothOffloadCb */
class BluetoothOffloadCallbacks:
      public vendor::qti::hardware::bluetooth_offload::V1_0::IBluetoothOffloadCb {
  public:
  BluetoothOffloadCallbacks() {
    LOG_INFO(LOG_TAG,"Constructor");
  }

  ~BluetoothOffloadCallbacks() {
    LOG_INFO(LOG_TAG,"Destructor");
  }

  Return<BtOffloadStatus> prepareOffload(PwrMode mode,
                     const hidl_vec<hidl_string>& remoteAddrs) {
    LOG_INFO(LOG_TAG,"Prepare Offload");
    std::vector<RawAddress> remote_devices;
    for (auto device : remoteAddrs) {
      remote_devices.emplace_back();
      RawAddress& bdaddr = remote_devices.back();
      RawAddress::FromString(device, bdaddr);
    }
    btif_lpm_prepare_offload_info((uint8_t) mode, remote_devices);
    btif_dispatch_lpm_event(BTIF_LPM_SM_CHECK_OFFLOAD_CMD,(void *)&mode, 0);
    return BtOffloadStatus::BT_OK;
  }

  Return<BtOffloadStatus> checkBtMsgActivity() {
    return is_bt_active_during_offload()? (BtOffloadStatus::BT_FAIL): (BtOffloadStatus::BT_OK);
  }

  Return<BtOffloadStatus> flowOff() {
    btif_dispatch_lpm_event(BTIF_LPM_SM_SEND_FLOW_OFF_CMD,NULL,0);
    return BtOffloadStatus::BT_OK;
  }

  Return<BtOffloadStatus> flowOn() {
    btif_dispatch_lpm_event(BTIF_LPM_SM_SEND_FLOW_ON_CMD,NULL,0);
    return BtOffloadStatus::BT_OK;
  }

  Return<BtOffloadStatus> commitOffload() {
    btif_dispatch_lpm_event(BTIF_LPM_SM_SEND_LPM_ENTER_CMD,NULL,0);
    return BtOffloadStatus::BT_OK;
  }

  Return<void> notifyPwrTransitionStatus(PwrMgrStatus status) {
    if (status == PwrMgrStatus::PWR_MGR_NACK)
      btif_dispatch_lpm_event(BTIF_LPM_SM_NOTIFY_NACK_MSG,NULL,0);
    return Void();
  }

  Return<BtOffloadStatus> disableOffload() {
    btif_dispatch_lpm_event(BTIF_LPM_SM_SEND_LPM_EXIT_CMD,NULL,0);
    return BtOffloadStatus::BT_OK;
  }
};

/*******************************************************************************
 *
 * Function         btif_lpm_native_interface_init
 *
 * Description      lpm native interface to initialize lpm native
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
bool btif_lpm_native_interface_init(void) {
  LOG_INFO(LOG_TAG,"btif_lpm_native_interface_init");
  btLpmNative = IBluetoothOffload::getService("bluetoothoffload", false);
  if (btLpmNative != nullptr)
  {
    btLpmNative->linkToDeath(lpmNativeDeathRecipient, 0);
    android::sp<BluetoothOffloadCallbacks::IBluetoothOffloadCb> btLpmNativeCb =
                                                 new BluetoothOffloadCallbacks();
    auto ret = btLpmNative->registerCb(btLpmNativeCb);
    if (!ret.isOk()) {
      LOG_ERROR(LOG_TAG, "hal server is dead ");
      btLpmNative = nullptr;
      return false;
    }
    btif_lpm_init_done();
    return true;
  }
  return false;
}

/*******************************************************************************
 *
 * Function         btif_lpm_native_interface_deinit
 *
 * Description      lpm native interface to de initialize lpm native
 *
 * Parameters       None
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_lpm_native_interface_deinit(void) {
  LOG_INFO(LOG_TAG,"btif_lpm_native_interface_deinit");
  if (btLpmNative != nullptr)
  {
    btLpmNative->unlinkToDeath(lpmNativeDeathRecipient);
    auto ret = btLpmNative->deregisterCb();
    if (!ret.isOk()) {
      LOG_ERROR(LOG_TAG, "hal server is dead ");
      return;
    }
    btLpmNative = nullptr;
  }
}
bool btif_lpm_native_interface_prepare_offload_done(
        LPMNativeStatus lpm_status, StackStatus stack_status) {
  if (lpm_status == LPM_FAILED && stack_status == OFFLOAD_FAILED) {
    auto ret = btLpmNative->prepareOffloadDone(BtOffloadAttemptStatus::BT_NOT_ALLOWED);
    if (!ret.isOk())
      return false;
  }

  if (lpm_status == LPM_SUCCESS && stack_status == OFFLOAD_FAILED) {
    auto ret = btLpmNative->prepareOffloadDone(BtOffloadAttemptStatus::BT_CPU_SUSPEND_OK);
    if (!ret.isOk())
      return false;
  }

  if (lpm_status == LPM_PENDING && stack_status == OFFLOAD_PENDING) {
    /*Fetch and send context_info*/
    auto ret = btLpmNative->prepareOffloadDone(BtOffloadAttemptStatus::BT_OFFLOAD_OK);
    if (!ret.isOk())
      return false;
  }
  return true;
}

bool btif_lpm_native_flow_on_ack_interface(uint8_t status) {
  BtOffloadStatus ret;
  if(status == 0x01) {
      ret = btLpmNative->flowOnDone(BtOffloadStatus::BT_OK);
  } else {
      ret = btLpmNative->flowOnDone(BtOffloadStatus::BT_FAIL);
  }
  if (ret != BtOffloadStatus::BT_OK)
      return false;
  return true;
}

bool btif_lpm_native_flow_off_ack_interface(uint8_t status) {
  BtOffloadStatus ret;
  if(status == 0x01) {
      ret = btLpmNative->flowOffDone(BtOffloadStatus::BT_OK);
  } else {
      ret = btLpmNative->flowOffDone(BtOffloadStatus::BT_FAIL);
  }
  if (ret != BtOffloadStatus::BT_OK)
      return false;
  return true;
}

bool btif_lpm_native_interface_commit_offload_done(
        LPMNativeStatus lpm_status, StackStatus stack_status) {
  if (lpm_status == LPM_SUCCESS && stack_status == OFFLOAD_PENDING) {
    auto ret = btLpmNative->commitOffloadDone(BtOffloadStatus::BT_OK);
    if (!ret.isOk())
      return false;
  } else {
    auto ret = btLpmNative->commitOffloadDone(BtOffloadStatus::BT_FAIL);
    if (!ret.isOk())
      return false;
  }
  return true;
}
bool btif_lpm_native_set_local_device_address(std::string addr, bool privacy) {
    LOG_INFO(LOG_TAG,"btif_lpm_native_set_local_device_address");
    auto ret = btLpmNative->setLocalDeviceAddress(addr, (privacy ?
               AddrType::ADDR_RANDOM :
               AddrType::ADDR_PUBLIC));
    if (!ret.isOk())
      return false;
  return true;
}

bool btif_lpm_native_set_local_device_name(std::string name) {
    LOG_INFO(LOG_TAG,"btif_lpm_native_set_local_device_name");
    auto ret = btLpmNative->setLocalDeviceName(name);
    if (!ret.isOk())
      return false;
  return true;
}
bool btif_lpm_native_set_le_context_info(btif_lpm_ble_conn_t& ble_conn, std::string dev_addr) {
    HciLeContextType leHciContext;
    GattContextType gattContext;
    std::vector<GattServiceInfo> gattServiceInfoVec;

    /*TODO: update islpmRegistered from offloadable app? */
    LOG_INFO(LOG_TAG,"btif_lpm_native_set_context_info");
    leHciContext.hci_LeHandle = ble_conn.hci_handle;
    //LOG_INFO(LOG_TAG, "hci handle:", ble_conn.hci_handle);

    if(ble_conn.conn_flags == CONN_ROLE_MASTER) {
        leHciContext.dir = ConnectionDirection::DIR_OUTGOING;
        LOG_INFO(LOG_TAG, "hci dir is outgoing");
    } else {
        leHciContext.dir = ConnectionDirection::DIR_INCOMING;
        LOG_INFO(LOG_TAG, "hci dir is incoming");
    }
    gattContext.mtu = ble_conn.mtu;
    LOG_INFO(LOG_TAG, "gatt mtu:%d", ble_conn.mtu);
     for (auto& gattServiceInfo : ble_conn.gatt_srvc) {
        if(gattServiceInfo.is_notify_reg){
            GattServiceInfo serviceInfo;
            serviceInfo.uuid  = gattServiceInfo.uuid.As16Bit();
            serviceInfo.start = gattServiceInfo.s_handle;
            serviceInfo.end   = gattServiceInfo.e_handle;
            gattServiceInfoVec.push_back(serviceInfo);
        }
    }

    btif_lpm_native_interface_send_le_hci_context_info((ble_conn.remote_addr).ToString(), leHciContext);
    btif_lpm_native_interface_send_le_gatt_context_info((ble_conn.remote_addr).ToString(), gattContext);
    btif_lpm_native_interface_send_le_gatt_handle_range_info((ble_conn.remote_addr).ToString(), gattServiceInfoVec);
    return true;
}
bool btif_lpm_native_interface_send_le_hci_context_info(
        std::string addr, HciLeContextType LeHciContext) {
    LOG_INFO(LOG_TAG,"btif_lpm_native_interface_send_le_hci_context_info");
    auto ret = btLpmNative->setLeHciContext(addr, LeHciContext);
    if (!ret.isOk())
      return false;
  return true;
}

bool btif_lpm_native_interface_send_le_gatt_context_info(
        std::string remoteAddr, GattContextType gattContext) {
    LOG_INFO(LOG_TAG,"btif_lpm_native_interface_send_le_gatt_context_info");
    auto ret = btLpmNative->setGattContext(remoteAddr, gattContext);
    if (!ret.isOk())
      return false;
  return true;
}

bool btif_lpm_native_interface_send_le_gatt_handle_range_info(
        std::string remoteAddr, std::vector<GattServiceInfo> serviceInfo) {
    LOG_INFO(LOG_TAG,"btif_lpm_native_interface_send_le_gatt_handle_range_info");
    auto ret = btLpmNative->setGattHandleRange(remoteAddr, serviceInfo);
    if (!ret.isOk())
      return false;
  return true;
}

bool btif_lpm_native_disable_offload_status_interface(LPMNativeStatus status) {
  btLpmNative->clearAll();

  BtOffloadStatus ret;
  if(status == 0x01) {
      ret = btLpmNative->disableOffloadDone(BtOffloadStatus::BT_OK);
  } else {
      ret = btLpmNative->disableOffloadDone(BtOffloadStatus::BT_FAIL);
  }
  if (ret != BtOffloadStatus::BT_OK)
      return false;
  return true;
}
}  // namespace V1_0
}  // namespace bluetooth_offload
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
