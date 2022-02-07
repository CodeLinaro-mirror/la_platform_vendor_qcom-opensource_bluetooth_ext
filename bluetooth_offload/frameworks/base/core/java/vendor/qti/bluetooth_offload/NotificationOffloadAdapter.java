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

package vendor.qti.bluetooth_offload;

import android.Manifest;
import android.app.ActivityThread;
import android.app.PropertyInvalidatedCache;
import android.annotation.NonNull;
import android.annotation.SuppressLint;
import android.compat.annotation.UnsupportedAppUsage;
import android.content.Context;
import android.os.Binder;
import android.os.IBinder;
import android.os.ParcelUuid;
import android.os.RemoteException;
import android.os.ResultReceiver;
import android.os.ServiceManager;
import android.os.SynchronousResultReceiver;
import android.os.SystemProperties;
import vendor.qti.bluetooth_offload.IBluetoothOffloadApp;
import vendor.qti.bluetooth_offload.IBluetoothOffloadAppCallback;
import vendor.qti.bluetooth_offload.INotificationOffloadMgr;
import vendor.qti.bluetooth_offload.INotificationOffloadMgrCallback;
import android.util.Log;

import java.util.List;
import java.util.concurrent.locks.ReentrantReadWriteLock;

/** @hide */
public final class NotificationOffloadAdapter {
    private static final String TAG = "NotificationOffloadAdapter";

    /** @hide */
    public static final String NOTIFICATION_OFFLOAD_MGR_SERVICE = "notification_offload_mgr";

    @UnsupportedAppUsage
    private INotificationOffloadMgr mManagerService;
    @UnsupportedAppUsage
    private IBluetoothOffloadApp mService;
    private Context mContext;

    private final ReentrantReadWriteLock mServiceLock = new ReentrantReadWriteLock();

    private String mApplicationId;
    @UnsupportedAppUsage
    private volatile BluetoothOffloadCallback mBluetoothOffloadCallback;
    /** @hide */
    public static final int BT_FAIL = 0;
    /** @hide */
    public static final int BT_OK = 1;
    /** @hide */
    public static final int BT_INVALID_STATE = 2;
    /** @hide */
    public static final int ACTIVE_MODE = 0;
    /** @hide */
    public static final int TRACKER_MODE = 1;
    /** @hide */
    public static final int POWER_STATUS_ACK = 0;
    /** @hide */
    public static final int POWER_STATUS_NACK = 1;

    private boolean mRegisterStatus = false;

    /**
     * Use {@link #getDefaultAdapter} to get the NotificationOffloadAdapter instance.
     */
    public NotificationOffloadAdapter(@NonNull Context context) {
        IBinder b = ServiceManager.getService(NOTIFICATION_OFFLOAD_MGR_SERVICE);
        if (b != null) {
            mManagerService = INotificationOffloadMgr.Stub.asInterface(b);

        if (mManagerService != null) {
            try {
                mServiceLock.writeLock().lock();
                mService = mManagerService.registerAdapter(mManagerCallback);
				mApplicationId = context.getPackageName();
            } catch (RemoteException e) {
                Log.e(TAG, "", e);
            } finally {
                mServiceLock.writeLock().unlock();
            }
        } else {
            Log.e(TAG, "notification offload manager service is null");
        }
        } else {
            Log.e(TAG, "NotificationOffloadMgr binder is null");
        }
    }
    /** @hide */
    @SuppressLint("ExecutorRegistration")
    public int register(@NonNull BluetoothOffloadCallback mCallback) {
        try {
            mServiceLock.readLock().lock();
            if (mService != null) {
				mBluetoothOffloadCallback = mCallback;
                int ret = mService.registerCb(mApplicationId, mBluetoothOffloadAppCallback);

                if (ret != BT_OK) {
                    Log.e(TAG, "start: registerCb failed with err=" + ret + " for app " +
                          mApplicationId);
                    return BT_FAIL;
                }

                mRegisterStatus = true;
                return BT_OK;
            }
        } catch (RemoteException e) {
            Log.e(TAG, "", e);
        } finally {
            mServiceLock.readLock().unlock();
        }
        return BT_FAIL;
    }

    /** @hide */
    public int unregister() {
        if (false == mRegisterStatus) {
            Log.i(TAG, "stop: failed (no service handle) for offloadableApp " +
                  mApplicationId);
            return BT_FAIL;
        }

        try {
            mServiceLock.readLock().lock();
            if (mService != null) {
                int ret = mService.unregisterCb(mApplicationId);

                if (ret != BT_OK) {
                    Log.e(TAG, "stop: unregisterCb failed with err=" + ret + " for app " +
                          mApplicationId);
                    return BT_FAIL;
                }

                mRegisterStatus = false;
                return BT_OK;
            }
        } catch (RemoteException e) {
            Log.e(TAG, "", e);
        } finally {
            mServiceLock.readLock().unlock();
        }
        return BT_FAIL;
    }

    /** @hide */
    public int enableOffloadDone(int status) {
        if (false == mRegisterStatus) {
            Log.e(TAG, "enableOffloadDone: failed (no service handle) for offloadableApp " +
                  mApplicationId);
            return BT_FAIL;
        }

        try {
            mServiceLock.readLock().lock();
            if (mService != null) {
                long ret = mService.enableOffloadDone(mApplicationId, status);
                if (BT_OK != ret) {
                    Log.e(TAG, "enableOffloadDone: failed with err=" + ret +
                          " for offloadableApp " + mApplicationId);
                    return BT_FAIL;
                }

                return BT_OK;
            }
        } catch (RemoteException e) {
            Log.e(TAG, "", e);
        } finally {
            mServiceLock.readLock().unlock();
        }
        return BT_FAIL;
    }

    /** @hide */
    public int disableOffloadDone(int status) {
        if (false == mRegisterStatus) {
            Log.e(TAG, "disableOffloadDone: failed (no service handle) for offloadableApp " +
                  mApplicationId);
            return BT_FAIL;
        }

        try {
            mServiceLock.readLock().lock();
            if (mService != null) {
                long ret = mService.disableOffloadDone(mApplicationId, status);
                if (BT_OK != ret) {
                    Log.e(TAG, "disableOffloadDone: failed with err=" + ret +
                          " for offloadableApp " + mApplicationId);
                    return BT_FAIL;
                }

                return BT_OK;
            }
        } catch (RemoteException e) {
            Log.e(TAG, "", e);
        } finally {
            mServiceLock.readLock().unlock();
        }
        return BT_FAIL;
    }

    /** @hide */
    public int setSubscribedGattHandles(@NonNull List<SubscribedGattHandles> subscribedHandles) {
        if (false == mRegisterStatus) {
            Log.e(TAG, "setSubscribedGattHandles: failed (no service handle)" +
                       " for " + "offloadableApp " + mApplicationId);
            return BT_FAIL;
        }

        Log.i(TAG, "setSubscribedGattHandles: app " + mApplicationId + " numGattHandles=" + subscribedHandles.size());

        try {
            mServiceLock.readLock().lock();
            if (mService != null) {
                long ret = mService.setSubscribedGattHandles(
                            mApplicationId, subscribedHandles);

                if (BT_OK != ret) {
                    Log.e(TAG, "setSubscribedGattHandles: failed with err=" + ret +
                          " for offloadableApp " + mApplicationId);
                    return BT_FAIL;
                }

                return BT_OK;
            }
        } catch (RemoteException e) {
            Log.e(TAG, "", e);
        } finally {
            mServiceLock.readLock().unlock();
        }
        return BT_FAIL;
    }

    /** @hide */
    public int setAppSpecificContextInfo(@NonNull byte[] blob) {
        if (false == mRegisterStatus) {
            Log.e(TAG, "setAppSpecificContextInfo: failed (no service handle)" +
                       " for " + "offloadableApp " + mApplicationId);
            return BT_FAIL;
        }

        try {
            mServiceLock.readLock().lock();
            if (mService != null) {
                if(null != blob) {
                    long ret = mService.setAppSpecificContextInfo(
                               mApplicationId, blob);

                    if (BT_OK != ret) {
                        Log.e(TAG, "setAppSpecificContextInfo: failed with err=" + ret +
                              " for offloadableApp " + mApplicationId);
                        return BT_FAIL;
                    }
                    return BT_OK;
                } else {
                    Log.e(TAG, "setAppSpecificContextInfo: failed, blob is null");
                }
            }
        } catch (RemoteException e) {
            Log.e(TAG, "", e);
        } finally {
            mServiceLock.readLock().unlock();
        }
        return BT_FAIL;
    }

    /** @hide */
    public void transitionToPwrState(int pwrState) {
        if (false == mRegisterStatus) {
            Log.e(TAG, "transitionToPwrState: failed (no service handle)");
            return;
        }

        try {
            mServiceLock.readLock().lock();
            if (mService != null) {
                mService.transitionToPwrState(pwrState);
            }
        } catch (RemoteException e) {
            Log.e(TAG, "", e);
        } finally {
            mServiceLock.readLock().unlock();
        }
    }

    /** @hide */
    public void setRfCommAppContextInfo (@NonNull String remoteAddrs, @NonNull String serviceId, @NonNull String appId) {
        if (false == mRegisterStatus) {
            Log.e(TAG, "setAppContextInfo: failed (no service handle)");
            return;
        }

        try {
            mServiceLock.readLock().lock();
            if (mService != null) {
                mService.setRfCommAppContextInfo(remoteAddrs, serviceId, appId);
            }
        } catch (RemoteException e) {
            Log.e(TAG, "", e);
        } finally {
            mServiceLock.readLock().unlock();
        }

    }

    /** @hide */
    public void acquire_pm_wakelock() {
        if (false == mRegisterStatus) {
            Log.e(TAG, "acquire_pm_wakelock: failed (no service handle)");
            return;
        }

        try {
            mServiceLock.readLock().lock();
            if (mService != null) {
                mService.acquire_pm_wakelock();
            }
        } catch (RemoteException e) {
            Log.e(TAG, "", e);
        } finally {
            mServiceLock.readLock().unlock();
        }
    }

    /** @hide */
    public void release_pm_wakelock() {
        if (false == mRegisterStatus) {
            Log.e(TAG, "release_pm_wakelock: failed (no service handle)");
            return;
        }

        try {
            mServiceLock.readLock().lock();
            if (mService != null) {
                mService.release_pm_wakelock();
            }
        } catch (RemoteException e) {
            Log.e(TAG, "", e);
        } finally {
            mServiceLock.readLock().unlock();
        }
    }

    private final INotificationOffloadMgrCallback mManagerCallback =
        new INotificationOffloadMgrCallback.Stub() {
                public void onBluetoothOffloadServiceUp(IBluetoothOffloadApp bluetoothOffloadService) {
                    Log.d(TAG, "onBluetoothOffloadServiceUp: " + bluetoothOffloadService);

                    mServiceLock.writeLock().lock();
                    mService = bluetoothOffloadService;
                    mServiceLock.writeLock().unlock();
                }

                public void onBluetoothOffloadServiceDown() {
                    Log.d(TAG, "onBluetoothOffloadServiceDown: " + mService);

                    try {
                        mServiceLock.writeLock().lock();
                        mService = null;
                    } finally {
                        mServiceLock.writeLock().unlock();
                    }

                    Log.d(TAG, "onBluetoothOffloadServiceDown");
                }
            };

    private final IBluetoothOffloadAppCallback mBluetoothOffloadAppCallback =
            new IBluetoothOffloadAppCallback.Stub() {
                /** @hide */
                public void notifyStartDone(int status) {
                    Log.d(TAG, "notifyStartDone for app " + mApplicationId);

                    if (mBluetoothOffloadCallback == null) {
                        Log.e(TAG, "notifyStartDone null callback for app " +
                              mApplicationId);
                        return;
                    }

                    mBluetoothOffloadCallback.onNotifyStartDone(status);
                }

                /** @hide */
                public void notifyStopDone(int status) {
                    Log.d(TAG, "notifyStopDone for app " + mApplicationId);

                    if (mBluetoothOffloadCallback == null) {
                        Log.e(TAG, "notifyStopDone null callback for app " +
                              mApplicationId);
                        return;
                    }

                    mBluetoothOffloadCallback.onNotifyStopDone(status);
                }

                /** @hide */
                public int notifyEnableOffload(int mode) {
                    Log.d(TAG, "notifyEnableOffload handler; mode=" + mode);
                    if (mBluetoothOffloadCallback == null) {
                        Log.e(TAG, "notifyEnableOffload failed null callback");
                        return BT_FAIL;
                    }

                    mBluetoothOffloadCallback.onNotifyEnableOffload(mode);
                    return BT_OK;
                }

                /** @hide */
                public int notifyDisableOffload(byte[] blob) {
                    Log.d(TAG, "notifyDisableOffload handler");
                    if (mBluetoothOffloadCallback == null) {
                        Log.e(TAG, "notifyDisableOffload failed null callback");
                        return BT_FAIL;
                    }

                    mBluetoothOffloadCallback.onNotifyDisableOffload(blob);
                    return BT_OK;
                }

                /** @hide */
                public void notifyAsyncErr(int status) {
                    Log.d(TAG, "notifyAsyncErr status " + status);
                    if (mBluetoothOffloadCallback != null) {
                        mBluetoothOffloadCallback.onNotifyAsyncErr(status);
                    }
                }

                /** @hide */
                public void transitionToPwrStateDone(int status) {
                    Log.d(TAG, "transitionToPwrStateDone status " + status);
                    if (mBluetoothOffloadCallback != null) {
                        mBluetoothOffloadCallback.onTransitionToPwrStateDone(status);
                    }
                }
            };
}
