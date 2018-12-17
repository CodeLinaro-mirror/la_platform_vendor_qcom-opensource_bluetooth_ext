/******************************************************************************

    Copyright (c) 2016, The Linux Foundation. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
        * Redistributions of source code must retain the above copyright
          notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above
          copyright notice, this list of conditions and the following
          disclaimer in the documentation and/or other materials provided
          with the distribution.
        * Neither the name of The Linux Foundation nor the names of its
          contributors may be used to endorse or promote products derived
          from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
    WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
    BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
    OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
    IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************/

/******************************************************************************
 *
 *  interface to aptX adaptive codec for sink.
 *
 ******************************************************************************/
#ifndef A2D_APTX_AD_H
#define A2D_APTX_AD_H

#include "a2dp_error_codes.h"
#include <pthread.h>
#include "osi/include/thread.h"

/* aptX codec specific settings*/
#define A2D_AVK_APTX_AD_CODEC_LEN        42
#define A2DP_AVK_APTX_AD_RESERVED_DATA   23

#define A2DP_AVK_APTX_AD_VENDOR_ID               (0x000000D7)
#define A2DP_AVK_APTX_AD_CODEC_ID_BLUETOOTH      (0x00AD)

#define A2DP_AVK_APTX_AD_SAMPLERATE_44100        (0x08)
#define A2DP_AVK_APTX_AD_SAMPLERATE_48000        (0x10)

#define A2DP_AVK_APTX_AD_CHANNELS_JOINT_STEREO  (0x08)

#define A2DP_AVK_APTX_AD_MIN_LATENCY_LL 10
#define A2DP_AVK_APTX_AD_MAX_LATENCY_LL 75
#define A2DP_AVK_APTX_AD_MIN_LATENCY_HQ 100
#define A2DP_AVK_APTX_AD_MAX_LATENCY_HQ 200
#define A2DP_AVK_APTX_AD_MIN_LATENCY_TWS 100
#define A2DP_AVK_APTX_AD_MAX_LATENCY_TWS 200

#define A2DP_AVK_APTX_AD_EOC0         0x00
#define A2DP_AVK_APTX_AD_EOC1         0x00
#define A2DP_AVK_APTX_AD_EOC2         0xAA

typedef struct {
  uint32_t vendorId;
  uint16_t codecId;    /* Codec ID for aptX-adaptive */
  uint8_t sampleRate;  /* Sampling Frequency */
  uint8_t channelMode;
  uint8_t min_latency_ll;
  uint8_t max_latency_ll;
  uint8_t min_latency_hq;
  uint8_t max_latency_hq;
  uint8_t min_latency_tws;
  uint8_t max_latency_tws;
  uint8_t eoc0;
  uint8_t eoc1;
  uint8_t eoc2;
  uint8_t reserved_data[A2DP_AVK_APTX_AD_RESERVED_DATA]; // 17th byte ( 3rd byte from starting should be AA)
  } tA2DP_AVK_APTX_AD_CIE;

uint8_t a2d_avk_aptx_ad_cfg_in_cap(uint8_t *p_cfg, tA2DP_AVK_APTX_AD_CIE *p_cap);
tA2DP_STATUS A2DP_ParseInfoAptxAdaptiveDecoder(tA2DP_AVK_APTX_AD_CIE *p_ie, uint8_t *p_info,
                                                    bool for_caps);
uint8_t A2D_BldAptxADDecoderInfo(uint8_t media_type, tA2DP_AVK_APTX_AD_CIE *p_ie,
                                                    uint8_t *p_result);

#endif /* A2D_APTX_H */
