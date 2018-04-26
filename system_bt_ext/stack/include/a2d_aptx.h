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
 *  interface to aptX codec
 *
 ******************************************************************************/
#ifndef A2D_APTX_H
#define A2D_APTX_H

/* aptX codec specific settings*/
#define A2D_APTX_CODEC_LEN        9

#define A2D_APTX_VENDOR_ID             (0x0000004F)
#define A2D_APTX_CODEC_ID_BLUETOOTH    (0x0001)
#define A2D_APTX_SAMPLERATE_MSK         (0xF0)
#define A2D_APTX_SAMPLERATE_44100       (0x20)
#define A2D_APTX_SAMPLERATE_48000       (0x10)
#define A2D_APTX_CHANNELS_MSK           (0x0F)
#define A2D_APTX_CHANNELS_STEREO        (0x02)
#define A2D_APTX_CHANNELS_MONO          (0x01)
#define A2D_APTX_FUTURE_1        (0x00)
#define A2D_APTX_FUTURE_2        (0x00)
#define A2D_APTX_OTHER_FEATURES_NONE  (0x00000000)
#define A2D_AV_APTX_AUDIO        (0x00)
#define A2D_APTX_CHANNEL         (0x0001)
#define A2D_APTX_SAMPLERATE      (0x22)

#include "a2dp_error_codes.h"
#include <pthread.h>
#include "osi/include/thread.h"


/*****************************************************************************
**  Type Definitions
*****************************************************************************/

typedef enum {
    APTX_CODEC_NONE=0,
    APTX_CODEC,
    APTX_HD_CODEC,
} A2D_AptXCodecType;

typedef void (*A2D_AptXThreadFn)(void *context);
typedef uint32_t (*A2D_AptXReadFn) (uint8_t ch_id, uint16_t *p_msg_evt, uint8_t* p_buf,
                                   uint32_t len);
typedef int (*A2D_AptXBufferSendFn) (uint8_t*, int, int);
typedef void (*A2D_AptXSetPriorityFn)(tHIGH_PRIORITY_TASK task);

typedef struct
{
    uint32_t vendorId;
    uint16_t codecId;         /* Codec ID for aptX */
    uint8_t  sampleRate;     /* Sampling Frequency */
    uint8_t  channelMode;    /* STEREO/DUAL/MONO */
    uint8_t  future1;
    uint8_t  future2;
} tA2D_APTX_CIE;

typedef struct  {
    int16_t s16SamplingFreq;  /* 16k, 32k, 44.1k or 48k*/
    int16_t s16ChannelMode;   /* mono, dual, streo or joint streo*/
    uint16_t u16BitRate;
    uint16_t *ps16NextPcmBuffer;
    uint8_t  *pu8Packet;
    uint8_t  *pu8NextPacket;
    uint16_t u16PacketLength;
    void* encoder;
} A2D_APTX_ENC_PARAMS;

extern const char* A2D_APTX_SCHED_LIB_NAME;
extern void *A2dAptXSchedLibHandle;
extern bool isA2dAptXEnabled;
extern thread_t *A2d_aptx_thread;
extern pthread_mutex_t aptx_thread_lock;
extern A2D_AptXThreadFn A2d_aptx_thread_fn;

/*****************************************************************************
**  external function declarations
*****************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif
extern uint8_t A2D_BldAptxInfo(uint8_t media_type, tA2D_APTX_CIE *p_ie,
                             uint8_t *p_result);
extern uint8_t A2D_ParsAptxInfo(tA2D_APTX_CIE *p_ie, uint8_t *p_info,
                              bool for_caps);
extern int (*A2D_aptx_encoder_init)(void);
extern A2D_AptXThreadFn (*A2D_aptx_sched_start)(void *encoder,
                          A2D_AptXCodecType aptX_codec_type,
                          bool use_SCMS_T, bool is_24bit_audio,
                          uint16_t sample_rate, uint8_t format_bits,
                          uint8_t channel, uint16_t mtu, A2D_AptXReadFn read_fn,
                          A2D_AptXBufferSendFn send_fn,
                          A2D_AptXSetPriorityFn set_priority_fn,
                          bool test, bool trace);
extern bool (*A2D_aptx_sched_stop)(void);
extern void (*A2D_aptx_encoder_deinit)(void);
extern uint8_t a2d_av_aptx_cfg_in_cap(uint8_t *p_cfg, tA2D_APTX_CIE *p_cap);
extern bool A2D_check_and_init_aptX();
extern void A2D_deinit_aptX();
extern void A2D_close_aptX();

#ifdef __cplusplus
}
#endif

#endif /* A2D_APTX_H */
