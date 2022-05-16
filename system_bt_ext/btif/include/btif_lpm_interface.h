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

}  // namespace V1_0
}  // namespace bluetooth_offload
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
#endif /* BTIF_LPM_INTERFACE_H  */
