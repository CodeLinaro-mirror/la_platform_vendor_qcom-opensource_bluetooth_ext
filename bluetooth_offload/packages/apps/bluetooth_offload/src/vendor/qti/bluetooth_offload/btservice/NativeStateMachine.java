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

import android.util.Log;

import android.content.Context;

import android.os.Build;
import android.os.SystemClock;
import android.os.SystemProperties;
import android.os.Message;

import java.util.UUID;
import java.util.Arrays;
import java.lang.*;
import java.io.*;

import libcore.io.IoUtils;

import com.android.internal.util.IState;
import com.android.internal.util.State;
import com.android.internal.util.StateMachine;

public class NativeStateMachine {
    private static final String TAG = "LPM_SM";
    public static int LOG_LEVEL = 6;
    private Context mcontext = null;

    public static final int AON_BT_FAIL = 0;
    public static final int AON_BT_SUCCESS = 1;
    public static NotificationOffloadStateMachine mStateMachine;
    private static BluetoothOffloadNativeInterface mNativeInterface = null;
    private static BluetoothOffloadService mBluetoothOffloadService = null;

    public NativeStateMachine(Context mcontext, BluetoothOffloadNativeInterface NativeIf) {
        Log.i(TAG, ": start");
        this.mcontext = mcontext;
        /* Initialize classes */
        mStateMachine = new NotificationOffloadStateMachine(mcontext);
        mNativeInterface = NativeIf;
        if(mNativeInterface == null) {
            Log.i(TAG, "mNativeInterface - null");
        } else {
            Log.i(TAG, "mNativeInterface not null");
        }
        mBluetoothOffloadService = BluetoothOffloadService.getBluetoothOffloadService();
    }

    public void cleanUp() {
        Log.i(TAG, "cleanUp");
        mStateMachine.quitNow();
        mStateMachine = null;
    }

    public class NotificationOffloadStateMachine extends StateMachine {

        public static final int MSG_LN_SM_ACTIVE = 0;
        public static final int MSG_LN_SM_TRACKER = 1;
        public static final int MSG_LN_SM_TIME_EXPIRED = 2;
        public static final int MSG_LN_SM_SEND_PREPARE_OFFLOAD = 3;
        public static final int MSG_LN_SM_APP_SPECIFIC_INFO = 4;
        public static final int MSG_LN_SM_PREPARE_OFFLOAD_DONE = 5;
        public static final int MSG_LN_SM_NO_BT_ACTIVITY = 6;
        public static final int MSG_LN_SM_FLOW_OFF_DONE = 7;
        public static final int MSG_LN_SM_FLOW_OFF_FAILED = 8;
        public static final int MSG_LN_SM_BT_ACTIVITY_ON = 9;
        public static final int MSG_LN_SM_BT_ACTIVITY_OFF = 10;
        public static final int MSG_LN_SM_COMMIT_OFFLOAD_DONE = 11;
        public static final int MSG_LN_SM_SLATE_OFFLOAD_ENABLED = 12;
        public static final int MSG_LN_SM_SLATE_OFFLOAD_ENABLE_FAILED = 13;
        public static final int MSG_LN_SM_FLOW_ON_DONE = 14;
        public static final int MSG_LN_DISABLE_OFFLOAD_DONE = 15;
        public static final int MSG_LN_SM_ATHERTON_WAKEUP_EVENT = 16;
        public static final int MSG_LN_SM_PREPARE_OFFLOAD_FAILED = 17;
        public static final int MSG_LN_CPU_SUSPEND_ONLY = 18;
        public static final int MSG_LN_SM_TWM = 19;
        public static final int MSG_LN_SM_DS = 20;
        public static final int POWER_STATUS_NACK = 0;
        public static final int POWER_STATUS_ACK = 1;

        /* LPM Native Connection states.*/
        private LNStackActive mStackActive;
        private LNAppsPending mAppsPending;
        private LNStackOffloadPending mStackOffloadPending;
        private LNSocOffloadPending mSocOffloadPending;
        private LNOffloaded mOffloaded;
        private LNActivePending mActivePending;
        private Context mContext;

        StringBuilder PrintStr = new StringBuilder();

        private NotificationOffloadStateMachine(Context context) {
            super("NotificationOffloadStateMachine");
            mContext = context;

            mStateMachine = this;
            mStackActive = new LNStackActive();
            mAppsPending = new LNAppsPending();
            mStackOffloadPending = new LNStackOffloadPending();
            mSocOffloadPending = new LNSocOffloadPending();
            mOffloaded = new LNOffloaded();
            mActivePending = new LNActivePending();

            addState(mStackActive);
            addState(mAppsPending);
            addState(mStackOffloadPending);
            addState(mSocOffloadPending);
            addState(mOffloaded);
            addState(mActivePending);

            setInitialState(mStackActive);
        }

        private class LNStackActive extends State {
            private static final String TAG = "LPM_SM:LNStackActive";

            @Override
            public void enter() {
                removeDeferredMessages(MSG_LN_SM_ACTIVE);
                if (hasDeferredMessages(MSG_LN_SM_TWM) || hasDeferredMessages(MSG_LN_SM_TRACKER)
                  || hasDeferredMessages(MSG_LN_SM_DS)) {
                    Log.i(TAG, "Native State Machine entered active, not handling any transitions, retry if needed");
                    mBluetoothOffloadService.transitionToPwrStateDone(POWER_STATUS_NACK);
                    removeDeferredMessages(MSG_LN_SM_TWM);
                    removeDeferredMessages(MSG_LN_SM_TRACKER);
                    removeDeferredMessages(MSG_LN_SM_DS);
                }
                Log.i(TAG, "Enter" + getCurrentMessage().what);
            }

            @Override
            public void exit() {
                Log.i(TAG, "Exit" + getCurrentMessage().what);
            }

            @Override
            public boolean processMessage(Message message) {
                Log.i(TAG, "processMessage: " + message.what);

                boolean retValue = HANDLED;
                boolean no_apps_register = false;
                switch (message.what) {
                    case MSG_LN_SM_ACTIVE:
                        Log.d(TAG,"mstackActive Device already in active state, so sending success");
                        mBluetoothOffloadService.transitionToPwrStateDone(POWER_STATUS_ACK);
                        break;
                    case MSG_LN_SM_TRACKER:
                        if(no_apps_register) {
                            Log.d(TAG,"mstackActive");
                            //send cpu suspend ok to power manager
                            mBluetoothOffloadService.transitionToPwrStateDone(POWER_STATUS_ACK);
                        } else {
                            mBluetoothOffloadService.notifyEnableOffload(mNativeInterface.TRACKER_MODE);
                            Log.i(TAG,"MSG_LN_SM_TRACKER, in else");
                            mNativeInterface.SendtransitionToPwrState(mNativeInterface.TRACKER_MODE);
                            transitionTo(mAppsPending);
                        }
                        break;
                    case MSG_LN_SM_TWM:
                        if(no_apps_register) {
                           Log.d(TAG,"mstackActive");
                           //send cpu suspend ok to power manager
                           mBluetoothOffloadService.transitionToPwrStateDone(POWER_STATUS_ACK);
                        } else {
                           mBluetoothOffloadService.notifyEnableOffload(mNativeInterface.TWM_MODE);
                            Log.i(TAG,"MSG_LN_TWM, in else");
                            mNativeInterface.SendtransitionToPwrState(mNativeInterface.TWM_MODE);
                            transitionTo(mAppsPending);
                            mStateMachine.sendMessage(
                                    NotificationOffloadStateMachine.MSG_LN_SM_SEND_PREPARE_OFFLOAD);
                        }
                        break;
                     case MSG_LN_SM_DS:
                          if (no_apps_register) {
                            Log.d(TAG,"mstackActive");
                            //send cpu suspend ok to power manager
                            mBluetoothOffloadService.transitionToPwrStateDone(POWER_STATUS_ACK);
                        } else {
                            mBluetoothOffloadService.notifyEnableOffload(mNativeInterface.TWM_MODE);
                            Log.i(TAG,"MSG_LN_DS, in else");
                            mNativeInterface.SendtransitionToPwrState(mNativeInterface.DS_MODE);
                            transitionTo(mAppsPending);
                            mStateMachine.sendMessage(
                                    NotificationOffloadStateMachine.MSG_LN_SM_SEND_PREPARE_OFFLOAD);
                        }
                        break;
                    default:
                        return NOT_HANDLED;
                }
                return retValue;
            }
   /*
            public class Timer implements Runnable {
                public void run(){
                    try {
                        Thread.sleep(1000);
                        mStateMachine.sendMessage(
                                    NotificationOffloadStateMachine.MSG_LN_SM_TIME_EXPIRED);
                    } catch(InterruptedException e){
                        Log.e(TAG, "error in thread sleep");
                    }
                }
            }
   */
        }

        private class LNAppsPending extends State {
            private static final String TAG = "LPM_SM:LNAppsPending";
            private boolean isPrepareOffloadSent = false;

            @Override
            public void enter() {
                Log.i(TAG, "Enter: " + getCurrentMessage().what);
                isPrepareOffloadSent = false;
            }

            @Override
            public void exit() {
                Log.i(TAG, "Exit: " + getCurrentMessage().what);
            }

            @Override
            public boolean processMessage(Message message) {
                Log.i(TAG, "processMessage: " + message.what);
                boolean retValue = HANDLED;
                switch (message.what) {
                    case MSG_LN_SM_SEND_PREPARE_OFFLOAD:
                        if (!isPrepareOffloadSent) {
                            isPrepareOffloadSent = true;
                            int ret = mNativeInterface.SendPrepareOffload();
                            if (ret != AON_BT_SUCCESS) {
                                mBluetoothOffloadService.transitionToPwrStateDone(POWER_STATUS_NACK);
                                transitionTo(mStackActive);
                            } else {
                                transitionTo(mStackOffloadPending);
                            }
                        } else {
                          Log.i(TAG, "PrepareOffload already sent");
                        }
                        break;
                    case MSG_LN_SM_APP_SPECIFIC_INFO:
                        transitionTo(mStackActive);
                        break;
                    case MSG_LN_SM_TWM:
                    case MSG_LN_SM_ACTIVE:
                    case MSG_LN_SM_TRACKER:
                    case MSG_LN_SM_DS:
                        deferMessage(message);
                        break;
                    default:
                        return NOT_HANDLED;
                }
                return retValue;
            }
        }

        private class LNStackOffloadPending extends State {
            private static final String TAG = "LPM_SM:LNStackOffloadPending";

            @Override
            public void enter() {
                Log.i(TAG, "Enter: " + getCurrentMessage().what);
            }

            @Override
            public void exit() {
                Log.i(TAG, "Exit: " + getCurrentMessage().what);
            }

            @Override
            public boolean processMessage(Message message) {
                Log.i(TAG, "processMessage: " + message.what);

                boolean retValue = HANDLED;
                switch (message.what) {
                    case MSG_LN_SM_PREPARE_OFFLOAD_DONE:
                        mNativeInterface.checkBtMsgActivityStatus();
                        break;
                    case MSG_LN_SM_BT_ACTIVITY_OFF:
                        mNativeInterface.SendFlowOff();
                        transitionTo(mSocOffloadPending);
                        break;
                    case MSG_LN_SM_PREPARE_OFFLOAD_FAILED:
                    case MSG_LN_CPU_SUSPEND_ONLY:
                    case MSG_LN_SM_BT_ACTIVITY_ON:
                        mNativeInterface.notifyPwrTransitionDone(POWER_STATUS_NACK);
                        mBluetoothOffloadService.transitionToPwrStateDone(POWER_STATUS_NACK);
                        processDisableOffload();
                        transitionTo(mAppsPending);
                        break;
                    case MSG_LN_SM_TWM:
                    case MSG_LN_SM_ACTIVE:
                    case MSG_LN_SM_TRACKER:
                    case MSG_LN_SM_DS:
                        deferMessage(message);
                        break;
                    case MSG_LN_SM_SEND_PREPARE_OFFLOAD:
                        Log.d(TAG, "Duplicate prepareOffload");
                        break;
                    default:
                        Log.d(TAG, "Not handled");
                        return NOT_HANDLED;
                }
                return retValue;
            }

            private void processDisableOffload() {
                mBluetoothOffloadService.notifyDisableOffloadToAll(null);
            }
        }

        private class LNSocOffloadPending extends State {
            private static final String TAG = "LPM_SM:LNSocOffloadPending";

            @Override
            public void enter() {
                Log.i(TAG, "Enter: " + getCurrentMessage().what);
            }

            @Override
            public void exit() {
                Log.i(TAG, "Exit: " + getCurrentMessage().what);
            }

            @Override
            public boolean processMessage(Message message) {
                Log.i(TAG, "processMessage: " + message.what);
                boolean retValue = HANDLED;
                switch (message.what) {
                    case MSG_LN_SM_FLOW_OFF_DONE:
                        mNativeInterface.checkBtMsgActivityStatus();
                        break;
                    case MSG_LN_SM_FLOW_OFF_FAILED:
                        mNativeInterface.SendFlowOn();
                        break;
                    case MSG_LN_SM_BT_ACTIVITY_ON:
                        mNativeInterface.SendFlowOn();
                        break;
                    case MSG_LN_SM_BT_ACTIVITY_OFF:
                        mNativeInterface.SendCommitOffload();
                        break;
                    case MSG_LN_SM_COMMIT_OFFLOAD_DONE:
                         int status = mNativeInterface.SendEnableOffloadAonStack();
                         if( status == AON_BT_SUCCESS) {
                            mBluetoothOffloadService.transitionToPwrStateDone(POWER_STATUS_ACK);
                            transitionTo(mOffloaded);
                         } else if(status == AON_BT_FAIL) {
                            mBluetoothOffloadService.transitionToPwrStateDone(POWER_STATUS_NACK);
                            transitionTo(mActivePending);
                          }
                        break;
                    case MSG_LN_SM_FLOW_ON_DONE:
                        mBluetoothOffloadService.notifyDisableOffloadToAll(null);
                        mBluetoothOffloadService.transitionToPwrStateDone(POWER_STATUS_NACK);
                        transitionTo(mAppsPending);
                        break;
                    case MSG_LN_SM_TWM:
                    case MSG_LN_SM_ACTIVE:
                    case MSG_LN_SM_TRACKER:
                    case MSG_LN_SM_DS:
                        deferMessage(message);
                        break;
                    default:
                        return NOT_HANDLED;
                }
                return retValue;
            }
        }

        private class LNOffloaded extends State {
            private static final String TAG = "LPM_SM:LNOffloaded";

            @Override
            public void enter() {
                if (hasDeferredMessages(MSG_LN_SM_ACTIVE)) {
                    removeDeferredMessages(MSG_LN_SM_ACTIVE);
                    mStateMachine.sendMessage(
                                    NotificationOffloadStateMachine.MSG_LN_SM_ACTIVE);
                }
                if (hasDeferredMessages(MSG_LN_SM_TWM) || hasDeferredMessages(MSG_LN_SM_TRACKER)
                  || hasDeferredMessages(MSG_LN_SM_DS)) {
                    Log.i(TAG, "has deferred messages ");
                }
                removeDeferredMessages(MSG_LN_SM_TWM);
                removeDeferredMessages(MSG_LN_SM_TRACKER);
                removeDeferredMessages(MSG_LN_SM_DS);
                Log.i(TAG, "Enter: " + getCurrentMessage().what);
            }

            @Override
            public void exit() {
                Log.i(TAG, "Exit: " + getCurrentMessage().what);
            }

            @Override
            public boolean processMessage(Message message) {
                Log.i(TAG, "processMessage: " + message.what);
                boolean retValue = HANDLED;
                switch (message.what) {
                    case MSG_LN_SM_ACTIVE:
                        //check if synchronous call is working
                        //int status = mNativeInterface.SendDisableOffloadAonStack();
                        mNativeInterface.FetchAppData();
                        transitionTo(mActivePending);
                        break;
                    case MSG_LN_SM_ATHERTON_WAKEUP_EVENT:
                        transitionTo(mActivePending);
                        break;
                    case MSG_LN_SM_TWM:
                    case MSG_LN_SM_TRACKER:
                    case MSG_LN_SM_DS:
                        mBluetoothOffloadService.transitionToPwrStateDone(POWER_STATUS_ACK);
                        break;
                    default:
                        return NOT_HANDLED;
                }
                return retValue;
            }
        }

        private class LNActivePending extends State {
            private static final String TAG = "LPM_SM:LNActivePending";

            @Override
            public void enter() {
                mNativeInterface.SendDisableOffload();
                Log.i(TAG, "Enter: " + getCurrentMessage().what);
            }

            @Override
            public void exit() {
                Log.i(TAG, "Exit: " + getCurrentMessage().what);
            }

            @Override
            public boolean processMessage(Message message) {
                Log.i(TAG, "processMessage: " + message.what);
                boolean retValue = HANDLED;
                switch (message.what) {
                    case MSG_LN_DISABLE_OFFLOAD_DONE:
                        mBluetoothOffloadService.getAppSpecificContextInfo();
                        mBluetoothOffloadService.transitionToPwrStateDone(POWER_STATUS_ACK);
                        transitionTo(mAppsPending);
                        break;
                    case MSG_LN_SM_ACTIVE:
                        deferMessage(message);
                        break;
                    default:
                        return NOT_HANDLED;
                }
                return retValue;
            }
        }
    }

    protected void finalize() {

    }
}
