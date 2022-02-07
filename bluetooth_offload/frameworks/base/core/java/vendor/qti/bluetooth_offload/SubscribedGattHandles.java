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

import android.annotation.NonNull;

import android.os.Parcel;
import android.os.Parcelable;

/** @hide */
public final class SubscribedGattHandles implements Parcelable {
    private final String remoteAddr;
    private final int[] handles;

    public SubscribedGattHandles(@NonNull String btAddr, @NonNull int[] gattHandles) {
        remoteAddr = btAddr;
        handles = gattHandles;
    }

    private SubscribedGattHandles(@NonNull Parcel in) {
	remoteAddr = in.readString();
	handles = in.createIntArray();
    }

    /** @hide */
    @Override
    public void writeToParcel(@NonNull Parcel out, int flags) {
        out.writeString(remoteAddr);
        out.writeIntArray(handles);
    }

    /** @hide */
    @NonNull
    public String getRemoteAddr() {
        return remoteAddr;
    }

    /** @hide */
    @NonNull
    public int[] getGattHandles() {
        return handles;
    }

    /** @hide */
    @Override
    public int describeContents() {
        return 0;
    }
    /** @hide */
    public static final @NonNull Parcelable.Creator<SubscribedGattHandles> CREATOR = new Parcelable.Creator<SubscribedGattHandles>() {
        public SubscribedGattHandles createFromParcel(Parcel in) {
            return new SubscribedGattHandles(in);
        }

        public SubscribedGattHandles[] newArray(int size) {
            return new SubscribedGattHandles[size];
        }
    };
}

