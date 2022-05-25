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

import android.os.IBinder;
import android.annotation.Nullable;
import android.os.RemoteException;
import android.os.ServiceSpecificException;
import android.os.IInterface;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.util.Log;
import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;

import vendor.qti.bluetooth_offload.SubscribedGattHandles;
import vendor.qti.bluetooth_offload.IBluetoothOffloadApp;
import vendor.qti.bluetooth_offload.IBluetoothOffloadAppCallback;

import java.util.List;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Objects;
import java.util.ArrayList;
import java.util.Arrays;

public class BluetoothOffloadService extends Service {
    public static final String TAG = "BluetoothOffloadService";
  private static final boolean DBG = true;

    public static final int BT_FAIL = 0;
    public static final int BT_OK = 1;
    public static final int BT_INVALID_STATE = 2;

    private final Object mLock = new Object();
    public static NativeStateMachine mNativeStateMachine = null;
    public static BluetoothOffloadNativeInterface mNativeInterface = null;

    HashMap<String, BluetoothOffloadAppInfo> mAppCbMap = new HashMap<String, BluetoothOffloadAppInfo>();

  private boolean mCleaningUp = false;

    private static BluetoothOffloadService sBluetoothOffloadService;

    public static synchronized BluetoothOffloadService getBluetoothOffloadService() {
        Log.d(TAG, "getBluetoothOffloadService() - returning " + sBluetoothOffloadService);
        return sBluetoothOffloadService;
    }

    private static synchronized void setBluetoothOffloadService(BluetoothOffloadService instance) {
        Log.d(TAG, "setBluetoothOffloadService() - trying to set service to " + instance);
        if (instance == null) {
            return;
        }
        sBluetoothOffloadService = instance;
    }

    private static synchronized void clearBluetoothOffloadService(BluetoothOffloadService current) {
        if (sBluetoothOffloadService == current) {
            sBluetoothOffloadService = null;
        }
    }



    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {

            Log.e(TAG,"inside on receive");
            String action = intent.getAction();
            if (BluetoothAdapter.ACTION_STATE_CHANGED.equals(action) &&
                             intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)
                                     == BluetoothAdapter.STATE_OFF) {
                 //destroy oject
                Log.e(TAG,"BT OFF - clearing object");
                mNativeInterface.deregisterCb();
                mNativeInterface = null;
                mNativeStateMachine = null;
            }
            else if (BluetoothAdapter.ACTION_STATE_CHANGED.equals(action) &&
                             intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)
                                     == BluetoothAdapter.STATE_ON) {
                 //create  oject
                Log.e(TAG,"BT ON - Init object");
                try {
                    Log.i(TAG, "start: BluetoothOffloadNativeInterface ");
                    if (mNativeInterface == null)
                        mNativeInterface = new BluetoothOffloadNativeInterface(getApplicationContext());
                } catch (Exception e) {
                     Log.e(TAG, "start: failed for BluetoothOffloadNativeInterface " +
                     ". exception " + e);
                }
                try {
                    if(mNativeStateMachine == null) {
                        mNativeStateMachine = new NativeStateMachine(getApplicationContext(), mNativeInterface);
                        mNativeStateMachine.mStateMachine.start();
                    }
                } catch (Exception e) {
                    Log.e(TAG, "start: failed for BluetoothOffloadNativeInterface " +
                    ". exception " + e);
                }
            }
        }
    };
    @Override
    public void onCreate() {
        super.onCreate();
        debugLog("onCreate()");
        mBinder = new BluetoothOffloadServiceBinder(this);
        setBluetoothOffloadService(this);
        registerReceiver(mReceiver,
                new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED));

        /* try {
            Log.i(TAG, "start: BluetoothOffloadNativeInterface ");
            mNativeInterface = new BluetoothOffloadNativeInterface();
        } catch (Exception e) {
            Log.e(TAG, "start: failed for BluetoothOffloadNativeInterface " +
                  ". exception " + e);
        }

        mNativeStateMachine = new NativeStateMachine(getApplicationContext(), mNativeInterface);
        mNativeStateMachine.mStateMachine.start(); */

        try {
            Log.i(TAG, "start: BluetoothOffloadNativeInterface ");
            if (mNativeInterface == null)
                mNativeInterface = new BluetoothOffloadNativeInterface(getApplicationContext());
        } catch (Exception e) {
            Log.e(TAG, "start: failed for BluetoothOffloadNativeInterface " +
                  ". exception " + e);
        }
        try {
            if(mNativeStateMachine == null) {
               mNativeStateMachine = new NativeStateMachine(getApplicationContext(), mNativeInterface);
               mNativeStateMachine.mStateMachine.start();
            }
        } catch (Exception e) {
            Log.e(TAG, "start: failed for BluetoothOffloadNativeInterface " +
                  ". exception " + e);
        }

        // if (null == mNativeInterface) {
            // Log.e(TAG, "start: Failed to get native interface " +
                  // applicationId);
            // return BT_FAIL;
        // }
    }

    @Override
    public IBinder onBind(Intent intent) {
        debugLog("onBind()");
        return mBinder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.w(TAG, "onUnbind, calling cleanup");
        cleanup();
        return super.onUnbind(intent);
    }

    @Override
    public void onDestroy() {
        debugLog("onDestroy()");
    }

    private boolean isAvailable() {
        return !mCleaningUp;
    }

    private BluetoothOffloadServiceBinder mBinder;

    private static class BluetoothOffloadServiceBinder extends IBluetoothOffloadApp.Stub {
        private BluetoothOffloadService mService;

        BluetoothOffloadServiceBinder(BluetoothOffloadService svc) {
            mService = svc;
        }

        public void cleanup() {
            mService = null;
        }

        public BluetoothOffloadService getService() {
            if (mService != null && mService.isAvailable()) {
                return mService;
            }
            return null;
        }

        @Override
        public int registerCb(String applicationId, IBluetoothOffloadAppCallback mOffloadableCb) {
            BluetoothOffloadService service = getService();
            if (service == null) {
                return BT_FAIL;
            }
            return service.registerCb(applicationId, mOffloadableCb);
        }

        @Override
        public int unregisterCb(String applicationId) {
            BluetoothOffloadService service = getService();
            if (service == null) {
                return BT_FAIL;
            }
            return service.unregisterCb(applicationId);
        }

        @Override
        public int enableOffloadDone(String applicationId, int status) {
            BluetoothOffloadService service = getService();
            if (service == null) {
                return BT_FAIL;
            }
            return service.enableOffloadDone(applicationId, status);
        }

        @Override
        public int disableOffloadDone(String applicationId, int status) {
            BluetoothOffloadService service = getService();
            if (service == null) {
                return BT_FAIL;
            }
            return service.disableOffloadDone(applicationId, status);
        }

        @Override
        public int setSubscribedGattHandles(String applicationId, List<SubscribedGattHandles> subscribedHandles) {
            BluetoothOffloadService service = getService();
            if (service == null) {
                return BT_FAIL;
            }
            return service.setSubscribedGattHandles(applicationId, subscribedHandles);
        }

        @Override
        public int setAppSpecificContextInfo(String applicationId, byte[] blob) {
            BluetoothOffloadService service = getService();
            if (service == null) {
                return BT_FAIL;
            }
            return service.setAppSpecificContextInfo(applicationId, blob);
        }

        @Override
        public void transitionToPwrState(int pwrState) {
            BluetoothOffloadService service = getService();
            if (service != null) {
                service.transitionToPwrState(pwrState);
            }
        }

        @Override
        public int setRfCommAppContextInfo(String remoteAddrs, String serviceId, String appId) {
            BluetoothOffloadService service = getService();
            if (service == null) {
                return BT_FAIL;
            }
            return service.setRfCommAppContextInfo(remoteAddrs, serviceId, appId);
        }

        @Override
        public void acquire_pm_wakelock() {
            BluetoothOffloadService service = getService();
            if (service != null) {
                service.acquire_pm_wakelock();
            }
        }

        @Override
        public void release_pm_wakelock() {
            BluetoothOffloadService service = getService();
            if (service != null) {
                service.release_pm_wakelock();
            }
        }
    }

    class BluetoothOffloadAppInfo {
        public BluetoothOffloadAppDeathRecipient deathRecipient;
        public IBluetoothOffloadAppCallback callback;
        public boolean isEnableOffloadDone = false;
        public boolean isDisableOffloadDone = false;

        BluetoothOffloadAppInfo(BluetoothOffloadAppDeathRecipient deathRecipient,
            IBluetoothOffloadAppCallback callback) {
            this.deathRecipient = deathRecipient;
            this.callback = callback;
        }
    }

    class BluetoothOffloadAppDeathRecipient implements IBinder.DeathRecipient {
        public String applicationId;

        BluetoothOffloadAppDeathRecipient(String appId) {
            this.applicationId = appId;
        }

        @Override
        public void binderDied() {
            if (DBG) {
                Log.d(TAG, "Binder is dead - unregistering OffloadableApp");
                unregisterCb(applicationId);
            }
        }
    }

    public int registerCb(String applicationId, IBluetoothOffloadAppCallback mOffloadableCb) {

        if (null == mOffloadableCb) {
            Log.i(TAG, "start: failed. null cb for offloadableApp " +
                  applicationId);
            return BT_FAIL;
        }
        try {
            synchronized (mLock) {
                BluetoothOffloadAppDeathRecipient deathRecipient = new BluetoothOffloadAppDeathRecipient(applicationId);
                IBinder binder = toBinder(mOffloadableCb);
            try {
                    binder.linkToDeath(deathRecipient, 0);
                } catch (RemoteException e) {
                    throw new IllegalArgumentException("Can't link to OffloadableApp's death");
                }
                BluetoothOffloadAppInfo appInfo = new BluetoothOffloadAppInfo(deathRecipient, mOffloadableCb);
                mAppCbMap.put(applicationId, appInfo);
                //Log.i(TAG, "cb added");
            }
            Log.i(TAG, "start: start " + applicationId);
            //ToDo: Send start app to LPM Native SM
            /*ret = mBtOffloadService.start(applicationId);
            if (ret != BT_OK) {
                Log.i(TAG, "start: start failed with err=" + ret + " for app " +
                      applicationId);
                return BT_FAIL;
            }*/
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "registerCb: failed for offloadableApp " + applicationId +
                ". exception " + e);
        }
        return BT_FAIL;
    }

    public int unregisterCb(String applicationId) {
        Log.i(TAG, "unregisterCb: %s" + applicationId);
        if (mAppCbMap.containsKey(applicationId)) {
            IBluetoothOffloadAppCallback mOffloadableCb = mAppCbMap.get(applicationId).callback;

            try {
                synchronized (mLock) {
                    IBinder binder = toBinder(mAppCbMap.get(applicationId).callback);
                    binder.unlinkToDeath(mAppCbMap.get(applicationId).deathRecipient, 0);
                    //mNativeInterface.deregisterCb();
                    mAppCbMap.remove(applicationId);
                }
                //ToDo: send stop app to LPM Native SM
                /*long ret = mBtOffloadService.stop(applicationId);
                if (BT_OK != ret) {
                    Log.e(TAG, "stop: failed with err=" + ret +
                          " for offloadableApp " + applicationId);
                    return BT_FAIL;
                }*/

                return BT_OK;
            } catch (Exception e) {
                Log.e(TAG, "stop: failed for offloadableApp " + applicationId +
                      ". exception " + e);
            }
            return BT_FAIL;
        } else {
          Log.e(TAG, "unregisterCb: failed app " + applicationId + " does not exist");
            return BT_FAIL;
        }
    }

    public int enableOffloadDone(String applicationId, int status) {
        Log.i(TAG, "enableOffloadDone: %s" + applicationId);
        if (mAppCbMap.containsKey(applicationId)) {
            BluetoothOffloadAppInfo appInfo = mAppCbMap.get(applicationId);
            if (appInfo != null) {
                appInfo.isEnableOffloadDone = true;

                synchronized (mLock) {
                    mAppCbMap.replace(applicationId, appInfo);
                }
                return BT_OK;
            }
        } else {
            Log.e(TAG, "enableOffloadDone: failed app " + applicationId + " does not exist");
        }

        return BT_FAIL;
    }

    public int disableOffloadDone(String applicationId, int status) {
        Log.i(TAG, "disableOffloadDone: %s" + applicationId);
        if (mAppCbMap.containsKey(applicationId)) {
            BluetoothOffloadAppInfo appInfo = mAppCbMap.get(applicationId);
            if (appInfo != null) {
                appInfo.isDisableOffloadDone = true;

                synchronized (mLock) {
                    mAppCbMap.replace(applicationId, appInfo);
                }
                return BT_OK;
            }
        } else {
            Log.e(TAG, "disableOffloadDone: failed app " + applicationId + " does not exist");
        }

        return BT_FAIL;
    }

    public int setRfCommAppContextInfo (String remoteAddr, String serviceId, String appId) {
        Log.i(TAG, "setRfCommAppContextInfo: app: " + appId + ", service UUID: " + serviceId + " , Remote Address: "+remoteAddr);
        try {

            long ret = mNativeInterface.setRfCommAppContextInfo(remoteAddr, serviceId, appId);

            if (BT_OK != ret) {
                Log.e(TAG, "setRfCommAppContextInfo: failed with err=" + ret +
                        " for offloadableApp " + appId);
                return BT_FAIL;
            }

            return BT_OK;

        } catch (Exception e) {
            Log.e(TAG, "setRfCommAppContextInfo: failed for offloadableApp " +
                    appId + ". exception " + e);
        }
        return BT_FAIL;
    }

   public int setSubscribedGattHandles(String applicationId, List<SubscribedGattHandles> subscribedHandles) {
        Log.i(TAG, "setSubscribedGattHandles: app " + applicationId);

        try {

            long ret = mNativeInterface.setSubscribedGattHandles(
                applicationId, subscribedHandles);

            if (BT_OK != ret) {
                Log.e(TAG, "setSubscribedGattHandles: failed with err=" + ret +
                      " for offloadableApp " + applicationId);
                return BT_FAIL;
            }

            return BT_OK;

        } catch (Exception e) {
            Log.e(TAG, "setSubscribedGattHandles: failed for offloadableApp " +
                  applicationId + ". exception " + e);
        }
        return BT_FAIL;
    }



    public int setAppSpecificContextInfo(String applicationId, byte[] blob) {
        Log.i(TAG, "setAppSpecificContextInfo: app " + applicationId);

        try {
            int ret = mNativeInterface.setAppSpecificContextInfo(
                applicationId, blob);

            if (BT_OK != ret) {
                Log.e(TAG, "setAppSpecificContextInfo: failed with err=" + ret +
                      " for offloadableApp " + applicationId);
                return BT_FAIL;
            }
            return BT_OK;
        } catch (Exception e) {
            Log.e(TAG, "setAppSpecificContextInfo: failed for offloadableApp " +
                  applicationId + ". exception " + e);
        }
        return BT_FAIL;
    }

    public void transitionToPwrState(int pwrState) {
        Log.i(TAG, "transitionToPwrState: state " + pwrState);

        if (pwrState == mNativeInterface.TRACKER_MODE) {
            NativeStateMachine.mStateMachine.sendMessage(
                      NativeStateMachine.NotificationOffloadStateMachine.MSG_LN_SM_TRACKER);
        } else if (pwrState == mNativeInterface.ACTIVE_MODE) {
            NativeStateMachine.mStateMachine.sendMessage(
                      NativeStateMachine.NotificationOffloadStateMachine.MSG_LN_SM_ACTIVE);
        } else if (pwrState == mNativeInterface.TWM_MODE) {
            NativeStateMachine.mStateMachine.sendMessage(
                      NativeStateMachine.NotificationOffloadStateMachine.MSG_LN_SM_TWM);
        }
    }

    public void acquire_pm_wakelock() {
        Log.i(TAG, "acquire_pm_wakelock");

        try {
            int ret = mNativeInterface.acquire_pm_wakelock();
            if (BT_OK != ret) {
                Log.e(TAG, "acquire_pm_wakelock: failed with err=" + ret);
            }
        } catch (Exception e) {
            Log.e(TAG, "acquire_pm_wakelock: failed with exception " + e);
        }
    }

    public void release_pm_wakelock() {
        Log.i(TAG, "release_pm_wakelock");

        try {
            int ret = mNativeInterface.release_pm_wakelock();
            if (BT_OK != ret) {
                Log.e(TAG, "release_pm_wakelock: failed with err=" + ret);
            }
        } catch (Exception e) {
            Log.e(TAG, "release_pm_wakelock: failed with exception " + e);
        }
    }

    IBinder toBinder(IBluetoothOffloadAppCallback e) {
        return ((IInterface) e).asBinder();
    }

    void cleanup() {
        debugLog("cleanup()");
        if (mCleaningUp) {
            errorLog("cleanup() - Service already starting to cleanup, ignoring request...");
            return;
        }

        mCleaningUp = true;

        if (mBinder != null) {
            mBinder.cleanup();
            mBinder = null;  //Do not remove. Otherwise Binder leak!
        }
    clearBluetoothOffloadService(this);
    }

    private void debugLog(String msg) {
        if (DBG) {
            Log.d(TAG, msg);
        }
    }

    private void errorLog(String msg) {
        Log.e(TAG, msg);
    }

    public int notifyEnableOffload(int mode) {
        Log.i(TAG, "notifyEnableOffload: mode " + mode);
        for (String appId : mAppCbMap.keySet())
        {
            BluetoothOffloadAppInfo appInfo = mAppCbMap.get(appId);
            if (appInfo != null) {
                appInfo.isEnableOffloadDone = false;

                synchronized (mLock) {
                    mAppCbMap.replace(appId, appInfo);
                    IBluetoothOffloadAppCallback mOffloadableCb = mAppCbMap.get(appId).callback;
                    try {
                        mOffloadableCb.notifyEnableOffload(mode);
                    } catch (Exception e) {
                        Log.e(TAG, "failed for notifyEnableOffload " +
                              ". exception " + e);
                    }
                }
            }
        }
        return BT_OK;
    }

    public int notifyDisableOffloadToAll(byte[] blob) {
        Log.i(TAG, "notifyDisableOffload");
        for (String appId : mAppCbMap.keySet())
        {
            BluetoothOffloadAppInfo appInfo = mAppCbMap.get(appId);
            if (appInfo != null) {
                appInfo.isDisableOffloadDone = false;

                synchronized (mLock) {
                    mAppCbMap.replace(appId, appInfo);
                    IBluetoothOffloadAppCallback mOffloadableCb = mAppCbMap.get(appId).callback;
                    try {
                        mOffloadableCb.notifyDisableOffload(blob);
                    } catch (Exception e) {
                        Log.e(TAG, "failed for notifyDisableOffload " +
                              ". exception " + e);
                    }
                }
            }
        }
        return BT_OK;
    }

    public int notifyDisableOffload(String appId, byte[] blob) {
        Log.i(TAG, "notifyDisableOffload " + appId);
      Log.d(TAG, "print blob :"+ Arrays.toString(blob) );

        BluetoothOffloadAppInfo appInfo = mAppCbMap.get(appId);
        if (appInfo != null) {
            appInfo.isDisableOffloadDone = false;

            synchronized (mLock) {
                mAppCbMap.replace(appId, appInfo);
                IBluetoothOffloadAppCallback mOffloadableCb = mAppCbMap.get(appId).callback;
                try {
                    mOffloadableCb.notifyDisableOffload(blob);
                } catch (Exception e) {
                    Log.e(TAG, "failed for notifyDisableOffload " +
                           ". exception " + e);
                }
            }
        }

        return BT_OK;
    }

    public void notifyAsyncErr(int status) {
        Log.i(TAG, "notifyAsyncErr: status " + status);
        for (String appId : mAppCbMap.keySet())
        {
            BluetoothOffloadAppInfo appInfo = mAppCbMap.get(appId);
            if (appInfo != null) {

                synchronized (mLock) {
                    IBluetoothOffloadAppCallback mOffloadableCb = mAppCbMap.get(appId).callback;
                    try {
                        mOffloadableCb.notifyAsyncErr(status);
                    } catch (Exception e) {
                        Log.e(TAG, "failed for notifyAsyncErr " +
                              ". exception " + e);
                    }
                }
            }
        }
    }

    public void transitionToPwrStateDone(int status) {
        Log.i(TAG, "transitionToPwrStateDone: status " + status);
        for (String appId : mAppCbMap.keySet())
        {
            BluetoothOffloadAppInfo appInfo = mAppCbMap.get(appId);
            if (appInfo != null) {

                synchronized (mLock) {
                    IBluetoothOffloadAppCallback mOffloadableCb = mAppCbMap.get(appId).callback;
                    try {
                        mOffloadableCb.transitionToPwrStateDone(status);
                    } catch (Exception e) {
                        Log.e(TAG, "failed for transitionToPwrStateDone " +
                              ". exception " + e);
                    }
                }
            }
        }
    }

    public void getAppSpecificContextInfo() {
        Log.i(TAG, "getAppSpecificContextInfo");
        for (String appId : mAppCbMap.keySet())
        {
            BluetoothOffloadAppInfo appInfo = mAppCbMap.get(appId);
            if (appInfo != null) {
                mNativeInterface.getAppSpecificContextInfo(appId);
            }
        }
    }

};


