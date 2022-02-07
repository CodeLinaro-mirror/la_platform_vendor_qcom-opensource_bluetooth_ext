/*
Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted (subject to the limitations in the
disclaimer below) provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package vendor.qti.bluetooth_offload.btservice;

import vendor.qti.bluetooth_offload.SubscribedGattHandles;

import android.content.Context;
import android.os.PowerManager;
import android.os.SystemClock;
import android.util.Log;

import vendor.qti.hardware.bluetooth_offload.V1_0.*;
import vendor.qti.hardware.bluetooth_offload.V1_0.GattHandlesSubscribed;


import java.util.List;
import java.io.IOException;
import java.io.ByteArrayOutputStream;
import java.io.ObjectOutputStream;
import java.io.*;
import java.util.*;

public class BluetoothOffloadNativeInterface {

    private static final String TAG = "BluetoothOffloadNativeInterface";
    public static final int BT_FAIL = 0;
    public static final int BT_OK = 1;
    public static final int BT_NOT_ALLOWED = 0;
    public static final int BT_CPU_SUSPEND_OK = 1;
    public static final int BT_OFFLOAD_OK = 2;

    public static final int BT_ACTIVE = 0;
    public static final int BT_INACTIVE = 1;
    public static final int BT_INVALID_STATE = 2;

    public static final int ACTIVE_MODE = 0;
    public static final int TRACKER_MODE = 1;

    private IBluetoothOffloadService mBtOffloadService;
    private PowerManager mPowerManager = null;

    public BluetoothOffloadNativeInterface(Context mContext) {
       Log.i(TAG, "Bluetooth Offload Native Interface");
        try {
            mBtOffloadService = IBluetoothOffloadService.getService();
            if (mBtOffloadService == null) {
                Log.e(TAG, "start: Failed to get mBtOffloadService");
                return;
            }else {
                Log.i(TAG, "BluetoothOffloadNativeInterface, mBtOffloadService");
                registerCb();
                if (mPowerManager == null) {
                    mPowerManager = mContext.getSystemService(PowerManager.class);
                    if (mPowerManager == null) {
                        Log.e(TAG, "not able to get PowerManager Service");
                    }
                } else {
                    Log.i(TAG, "BluetoothOffloadNativeInterface, PowerManager already initialized");
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "getService failed for BluetoothOffloadService " +
            ". exception " + e);
        }
    }

    public void sendMessageToSM(int message) {
        NativeStateMachine.mStateMachine.sendMessage(message);
    }

    public int registerCb() {
        Log.i(TAG, "register-cb1");
        try {
            int ret = mBtOffloadService.registerCb(mBtOffloadableServiceCb);

            if (BT_OK != ret) {
                Log.e(TAG, "registerCb failed");
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "registerCb: failed for BluetoothOffloadService " +
            ". exception " + e);
        }
        Log.i(TAG, "register-cb2");
        return BT_FAIL;
    }

    public void deregisterCb() {
        Log.i(TAG, "deregisterCb");
        mPowerManager = null;
        try {
            mBtOffloadService.deregisterCb();
        } catch (Exception e) {
            Log.e(TAG, "deregisterCb: failed for BluetoothOffloadService " +
            ". exception " + e);
        }
    }

    public int setRfCommAppContextInfo (String remoteAddrs, String serviceId, String appId) {
        Log.i(TAG, "setRfCommAppContextInfo");
        try {
            int ret = mBtOffloadService.setRfcommAppContextInfo(remoteAddrs, serviceId, appId);
            if (BT_OK != ret) {
                Log.e(TAG, "setRfCommAppContextInfo failed");
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "setRfCommAppContextInfo: failed for BluetoothOffloadService " +
                    ". exception " + e);
        }
        return BT_FAIL;
    }

    public int setSubscribedGattHandles(String appId, List<SubscribedGattHandles> subscribedHandles) {
         ArrayList<vendor.qti.hardware.bluetooth_offload.V1_0.GattHandlesSubscribed> subscribedHandlesList =
                new ArrayList<vendor.qti.hardware.bluetooth_offload.V1_0.GattHandlesSubscribed> ();
         Log.i(TAG, "setSubscribedGattHandles");
         for(SubscribedGattHandles subscribedHandle : subscribedHandles) {
           vendor.qti.hardware.bluetooth_offload.V1_0.GattHandlesSubscribed gattHandles =
     new vendor.qti.hardware.bluetooth_offload.V1_0.GattHandlesSubscribed();

           int[] handles = subscribedHandle.getGattHandles();
           gattHandles.remote_addr = String.valueOf(subscribedHandle.getRemoteAddr());
     Log.i(TAG, "setSubscribedGattHandles, remote address after copy:" + gattHandles.remote_addr);
           for (int handle: handles) {
               gattHandles.handles.add(handle);
           }
           subscribedHandlesList.add(gattHandles);
         }
         try {
            int ret = mBtOffloadService.setSubscribedGattHandles(appId, subscribedHandlesList);

            if (BT_OK != ret) {
                Log.e(TAG, "setSubscribedGattHandles failed");
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "setSubscribedGattHandles: failed for BluetoothOffloadService " +
            ". exception " + e);
        }
        return BT_FAIL;
    }

    public int setAppSpecificContextInfo(String appId, byte[] blob) {
        Log.i(TAG, "setAppSpecificContextInfo");
        ArrayList<Byte> blobList = new ArrayList<Byte>(blob.length);
  //for (int i = 0; i<blobList.size(); i++){
  // blobList.add(blob);
  //}
        for(byte b : blob){
            blobList.add(new Byte(b));
        }
        /* Vector<Integer> blobVec = new Vector<>();
        for(int i=0; i < blob.length; i++) {
            int Seg = blob[i];
            blobVec.add(Seg);
        }*/
        try {
            int ret = mBtOffloadService.setAppSpecificContextInfo(appId, blobList);

            if (BT_OK != ret) {
                Log.e(TAG, "setAppSpecificContextInfo failed");
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "setAppSpecificContextInfo: failed for BluetoothOffloadService " +
            ". exception " + e);
        }
        return BT_FAIL;
    }

    public int getAppSpecificContextInfo(String appId) {
        Log.i(TAG, "setAppSpecificContextInfo");
        try {
            int ret = mBtOffloadService.getAppSpecificContextInfo(appId);

            if (BT_OK != ret) {
                Log.e(TAG, "setAppSpecificContextInfo failed");
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "setAppSpecificContextInfo: failed for BluetoothOffloadService " +
            ". exception " + e);
        }
        return BT_FAIL;
    }

    public int SendPrepareOffload() {
        Log.i(TAG, "SendPrepareOffload");
        try {
            int ret = mBtOffloadService.SendPrepareOffload();

            if (BT_OK != ret) {
                Log.e(TAG, "SendPrepareOffload failed");
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "SendPrepareOffload: failed for BluetoothOffloadService " +
            ". exception " + e);
        }
        return BT_FAIL;
    }

    public int checkBtMsgActivityStatus() {
        Log.i(TAG, "checkBtMsgActivityStatus");
        try {
            int ret = mBtOffloadService.checkBtMsgActivityStatus();

            if (BT_OK != ret) {
                Log.e(TAG, "checkBtMsgActivityStatus failed");
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "checkBtMsgActivityStatus: failed for BluetoothOffloadService" +
            ". exception " + e);
        }
        return BT_FAIL;
    }

    public int SendFlowOff() {
        Log.i(TAG, "SendFlowOff");
        try {
            int ret = mBtOffloadService.SendFlowOff();

            if (BT_OK != ret) {
                Log.e(TAG, "SendFlowOff failed");
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "SendFlowOff: failed for BluetoothOffloadService " +
            ". exception " + e);
        }
        return BT_FAIL;
    }

    public int SendFlowOn() {
        Log.i(TAG, "SendFlowOn");
        try {
            int ret = mBtOffloadService.SendFlowOn();

            if (BT_OK != ret) {
                Log.e(TAG, "SendFlowOn failed");
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "SendFlowOn: failed for BluetoothOffloadService " +
            ". exception " + e);
        }
        return BT_FAIL;
    }

    public int SendCommitOffload() {
        Log.i(TAG, "SendCommitOffload");
        try {
            int ret = mBtOffloadService.SendCommitOffload();

            if (BT_OK != ret) {
                Log.e(TAG, "SendCommitOffload failed");
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "SendCommitOffload: failed for BluetoothOffloadService " +
            ". exception " + e);
        }
        return BT_FAIL;
    }

    public int SendDisableOffload() {
        Log.i(TAG, "SendDisableOffload");
        try {
            int ret = mBtOffloadService.SendDisableOffload();

            if (BT_OK != ret) {
                Log.e(TAG, "SendDisableOffload failed");
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "SendDisableOffload: failed for BluetoothOffloadService " +
            ". exception " + e);
        }
        return BT_FAIL;
    }

    public int SendEnableOffloadAonStack() {
        Log.i(TAG, "SendEnableOffloadAonStack");
        try {
            int ret = mBtOffloadService.SendEnableOffloadAonStack();

            if (BT_OK != ret) {
                Log.e(TAG, "SendEnableOffloadAonStack failed");
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "SendEnableOffloadAonStack: failed for BluetoothOffloadService " +
            ". exception " + e);
        }
        return BT_FAIL;
    }

    public int SendDisableOffloadAonStack() {
        Log.i(TAG, "SendDisableOffloadAonStack");
        try {
            int ret = mBtOffloadService.SendDisableOffloadAonStack();

            if (BT_OK != ret) {
                Log.e(TAG, "SendDisableOffloadAonStack failed");
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "SendDisableOffloadAonStack: failed for BluetoothOffloadService " +
            ". exception " + e);
        }
        return BT_FAIL;
    }

    public void notifyPwrTransitionDone(int status) {
        Log.i(TAG, "SendtransitionToPwrState, status:" + status);
        //ToDo: send to pwr mgr
        try {
            mBtOffloadableServiceCb.notifyPwrTransitionDone(status);
        } catch (Exception e) {
            Log.e(TAG, "notifyPwrTransitionDone: failed for mBtOffloadableServiceCb " +
            ". exception " + e);
        }
    }

    public int acquire_pm_wakelock() {
        Log.i(TAG, "acquire_pm_wakelock");
        try {
            mBtOffloadService.acquire_pm_wakelock();
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "acquire_pm_wakelock: failed for BluetoothOffloadService " +
                    ". exception " + e);
        }
        return BT_FAIL;
    }

    public int release_pm_wakelock() {
        Log.i(TAG, "release_pm_wakelock");
        try {
            mBtOffloadService.release_pm_wakelock();
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "release_pm_wakelock: failed for BluetoothOffloadService " +
            ". exception " + e);
        }
        return BT_FAIL;
    }

    // Callbacks from the lpm native to the Java framework.
    // All callbacks are routed from here to LPM Native SM

    private final IBluetoothOffloadServiceCb.Stub mBtOffloadableServiceCb =
      new IBluetoothOffloadServiceCb.Stub() {

        public void notifyAppSpecificContextInfo(String appId, java.util.ArrayList<Byte> blob) {
            Log.i(TAG, "notifyAppSpecificContextInfo");

            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            ObjectOutputStream oos;
      try {
       oos = new ObjectOutputStream(baos);
       oos.writeObject(blob);
      } catch (IOException e) {
       e.printStackTrace();
      }
      // Convert to Byte Array
      //byte[] byteArray = baos.toByteArray();

            BluetoothOffloadService mBluetoothOffloadService =
                BluetoothOffloadService.getBluetoothOffloadService();
   //byte[] byteArray = new byte[blob.size()];
   //blob.toArray(byteArray);

   int size = blob.size();
            byte[] byteArray = new byte[size];
            Byte[] temp = blob.toArray(new Byte[size]);
            for (int n = 0; n < size; ++n) {
                byteArray[n] = temp[n];
            }


            mBluetoothOffloadService.notifyDisableOffload(appId, byteArray);
      //Check where blob Parser needs to be placed, ??
            sendMessageToSM(
                    NativeStateMachine.NotificationOffloadStateMachine.MSG_LN_SM_APP_SPECIFIC_INFO);
        }

        public void notifyAsyncErr(int status) {
            Log.i(TAG, "notifyAsyncErr");
        }

        public int notifyPrepareOffloadDone(int status) {
            Log.i(TAG, "notifyPrepareOffloadDone, status:" + status);
            if (status == BT_OFFLOAD_OK) {
                sendMessageToSM(
                    NativeStateMachine.NotificationOffloadStateMachine.MSG_LN_SM_PREPARE_OFFLOAD_DONE);
            } else {
                sendMessageToSM(
                    NativeStateMachine.NotificationOffloadStateMachine.MSG_LN_SM_PREPARE_OFFLOAD_FAILED);
            }
            return BT_OK;
        }

        public int notifyFlowOffDone(int status) {
            Log.i(TAG, "notifyFlowOffDone, status:" + status);
            sendMessageToSM(
                NativeStateMachine.NotificationOffloadStateMachine.MSG_LN_SM_FLOW_OFF_DONE);
            return BT_OK;
        }

        public int notifyFlowOnDone(int status) {
            Log.i(TAG, "notifyFlowOnDone, status:" + status);
            sendMessageToSM(
                NativeStateMachine.NotificationOffloadStateMachine.MSG_LN_SM_FLOW_ON_DONE);
            return BT_OK;
        }

        public int notifyCommitOffloadDone(int status) {
            Log.i(TAG, "notifyCommitOffloadDone, status:" + status);
            sendMessageToSM(
                NativeStateMachine.NotificationOffloadStateMachine.MSG_LN_SM_COMMIT_OFFLOAD_DONE);
            return BT_OK;
        }

        public int notifyDisableOffloadDone(int status) {
            Log.i(TAG, "notifyDisableOffloadDone, status:" + status);
            sendMessageToSM(
                NativeStateMachine.NotificationOffloadStateMachine.MSG_LN_DISABLE_OFFLOAD_DONE);
            return BT_OK;
        }

        public void notifyAthWakeupEvent() {
            Log.i(TAG, "notifyAthWakeupEvent");
            if (mPowerManager != null) {
                Log.i(TAG, "sending PowerManager wakeup event");
                mPowerManager.wakeUp(SystemClock.uptimeMillis(), PowerManager.WAKE_REASON_APPLICATION,
                        "notifyAthWakeup");
            }
            sendMessageToSM(
                NativeStateMachine.NotificationOffloadStateMachine.MSG_LN_SM_ATHERTON_WAKEUP_EVENT);
        }

        public int notifyBtMsgActivityStatus(int status) {
            Log.i(TAG, "notifyBtMsgActivityStatus, status:" + status);
            if (status == BT_ACTIVE) {
                sendMessageToSM(
                    NativeStateMachine.NotificationOffloadStateMachine.MSG_LN_SM_BT_ACTIVITY_ON);
            } else if (status == BT_INACTIVE){
                sendMessageToSM(
                    NativeStateMachine.NotificationOffloadStateMachine.MSG_LN_SM_BT_ACTIVITY_OFF);
            }
            return BT_OK;
        }

        public void notifyPwrTransitionDone(int status) {
            Log.i(TAG, "notifyPwrTransitionDone, status:" + status);
        }
    };
}
