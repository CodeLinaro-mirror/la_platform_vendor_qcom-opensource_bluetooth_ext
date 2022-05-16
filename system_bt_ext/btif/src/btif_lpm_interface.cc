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
    return BtOffloadStatus::BT_OK;
  }

  Return<BtOffloadStatus> checkBtMsgActivity() {
    return BtOffloadStatus::BT_OK;
  }

  Return<BtOffloadStatus> flowOff() {
    return BtOffloadStatus::BT_OK;
  }

  Return<BtOffloadStatus> flowOn() {
    return BtOffloadStatus::BT_OK;
  }

  Return<BtOffloadStatus> commitOffload() {
    return BtOffloadStatus::BT_OK;
  }

  Return<void> notifyPwrTransitionStatus(PwrMgrStatus status) {
    return Void();
  }

  Return<BtOffloadStatus> disableOffload() {
    return BtOffloadStatus::BT_OK;
  }
};

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
}  // namespace V1_0
}  // namespace bluetooth_offload
}  // namespace hardware
}  // namespace qti
}  // namespace vendor