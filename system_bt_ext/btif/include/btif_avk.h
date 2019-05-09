/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/*******************************************************************************
 *
 *  Filename:      btif_av.h
 *
 *  Description:   Main API header file for all BTIF AV functions accessed
 *                 from internal stack.
 *
 *******************************************************************************/

#ifndef BTIF_AVK_H
#define BTIF_AVK_H

#include "btif_common.h"
#include "btif_sm.h"
#include "bta_avk_api.h"
#include "btif/include/btif_api.h"

#define BTIF_AVK_RC_HANDLE_NONE 0xFF

/*******************************************************************************
**  Type definitions for callback functions
********************************************************************************/

typedef enum {
    /* Reuse BTA_AVK_XXX_EVT - No need to redefine them here */
    BTIF_AVK_CONNECT_REQ_EVT = BTA_AVK_MAX_EVT,
    BTIF_AVK_DISCONNECT_REQ_EVT,
    BTIF_AVK_START_STREAM_REQ_EVT, // this directly sends AVDTP_START
    BTIF_AVK_SINK_START_STREAM_REQ_EVT, // if avrcp connection sends AVRCP_PLAY , othwewise AVDTP_START
    BTIF_AVK_STOP_STREAM_REQ_EVT,
    BTIF_AVK_SUSPEND_STREAM_REQ_EVT,
    BTIF_AVK_SINK_SUSPEND_STREAM_REQ_EVT,
    BTIF_AVK_SINK_CONFIG_REQ_EVT,
    BTIF_AVK_CLEANUP_REQ_EVT,
    BTIF_AVK_SINK_FOCUS_REQ_EVT,
    BTIF_AVK_SPLIT_SINK_START_IND_RSP,
    BTIF_AVK_SPLIT_SINK_SUSPEND_IND_RSP,
    BTIF_AVK_SPLIT_SINK_START_CAPTURE,
    BTIF_AVK_SPLIT_SINK_STOP_CAPTURE,
    BTIF_AVK_SPLIT_SINK_CHECK_A2DP_READY,
    BTIF_AVK_SPLIT_SINK_SETUP_COMPLETE,
    BTIF_AVK_SPLIT_SINK_SEND_VSC_A2DP_START,
    BTIF_AVK_SPLIT_SINK_SEND_VSC_A2DP_STOP,
    BTIF_AVK_SPLIT_SINK_VSC_START_COMPLETE_EVT,
    BTIF_AVK_SPLIT_SINK_VSC_STOP_COMPLETE_EVT,
    BTIF_AVK_SPLIT_PENDING_START_ALARM_TIMEOUT,
    BTIF_AVK_SPLIT_PENDING_SUSPEND_ALARM_TIMEOUT,
    BTIF_AVK_INIT_REQ_EVT,
} btif_avk_sm_event_t;


/*******************************************************************************
**  BTIF AV API
********************************************************************************/

/*******************************************************************************
**
** Function         btif_avk_get_sm_handle
**
** Description      Fetches current av SM handle
**
** Returns          None
**
*******************************************************************************/

btif_sm_handle_t btif_avk_get_sm_handle(void);

/*******************************************************************************
**
** Function         btif_avk_get_addr
**
** Description      Fetches current AV BD address
**
** Returns          BD address
**
*******************************************************************************/

bt_bdaddr_t btif_avk_get_addr(RawAddress address);

/*******************************************************************************
**
** Function         btif_avk_stream_ready
**
** Description      Checks whether AV is ready for starting a stream
**
** Returns          None
**
*******************************************************************************/

bool btif_avk_stream_ready(void);

/*******************************************************************************
**
** Function         btif_avk_stream_started_ready
**
** Description      Checks whether AV ready for media start in streaming state
**
** Returns          None
**
*******************************************************************************/

bool btif_avk_stream_started_ready(void);

/*******************************************************************************
**
** Function         btif_avk_dispatch_sm_event
**
** Description      Send event to AV statemachine
**
** Returns          None
**
*******************************************************************************/

/* used to pass events to AV statemachine from other tasks */
void btif_avk_dispatch_sm_event(btif_avk_sm_event_t event, void *p_data, int len);

/*******************************************************************************
**
** Function         btif_avk_split_dispatch_sm_event
**
** Description      Send event to AV statemachine of a2dp split sink
**
** Returns          None
**
*******************************************************************************/
void btif_avk_split_dispatch_sm_event(btif_avk_sm_event_t event, void *p_data, int len);

/*******************************************************************************
**
** Function         btif_avk_split_get_started_device_idx
**
** Description      updates index of device which is in started state.
**
** Returns          TRUE: if only 1 device is in started/suspending/VSC handling state
**                  FALSE otherwise
**
**
*******************************************************************************/
bool btif_avk_split_get_started_device_idx(uint8_t* index);

/*******************************************************************************
**
** Function         is_streaming_bda_empty
**
** Description      checks if streaming_bda is empty(split).
**
** Returns          TRUE: if streaming_bda is NULL
**
**
*******************************************************************************/
bool is_streaming_bda_empty(void);

/*******************************************************************************
**
** Function         btif_avk_init
**
** Description      Initializes btif AV if not already done
**
** Returns          bt_status_t
**
*******************************************************************************/

bt_status_t btif_avk_init(int service_id);

/*******************************************************************************
**
** Function         btif_avk_is_connected
**
** Description      Checks if av has a connected sink
**
** Returns          bool
**
*******************************************************************************/

bool btif_avk_is_connected(void);


/*******************************************************************************
**
** Function         btif_avk_is_peer_edr
**
** Description      Check if the connected a2dp device supports
**                  EDR or not. Only when connected this function
**                  will accurately provide a true capability of
**                  remote peer. If not connected it will always be false.
**
** Returns          TRUE if remote device is capable of EDR
**
*******************************************************************************/

bool btif_avk_is_peer_edr(void);

#ifdef USE_AUDIO_TRACK
/*******************************************************************************
**
** Function         btif_avk_queue_focus_rquest
**
** Description      This is used to move context to btif and
**                  queue audio_focus_request
**
** Returns          none
**
*******************************************************************************/
void btif_avk_queue_focus_rquest(void);
#endif

/*******************************************************************************
 **
 ** Function         btif_media_enque_sink_data
 **
 ** Description      queues PCM data
 **
 ** Returns          void
 **
 *******************************************************************************/
uint32_t btif_media_enque_sink_data(uint16_t codec_type, uint8_t *data, uint16_t size, RawAddress bd_addr, uint64_t enque_time);

/*******************************************************************************
 **
 ** Function         btif_media_avk_fetch_pcm_data
 **
 ** Description      fetch PCM data
 **
 ** Returns          size of data read
 **
 *******************************************************************************/
uint32_t btif_media_avk_fetch_pcm_data(uint16_t codec_type, uint8_t *data, uint32_t size);

/******************************************************************************
**
** Function         btif_avk_clear_remote_suspend_flag
**
** Description      Clears remote suspended flag
**
** Returns          Void
********************************************************************************/

void btif_avk_clear_remote_suspend_flag(void);

/*******************************************************************************
**
** Function         btif_avk_peer_supports_3mbps
**
** Description      Check if the connected a2dp device supports
**                  3mbps edr. Only when connected this function
**                  will accurately provide a true capability of
**                  remote peer. If not connected it will always be false.
**
** Returns          TRUE if remote device is EDR and supports 3mbps
**
*******************************************************************************/
bool btif_avk_peer_supports_3mbps(void);

/*******************************************************************************
**
** Function         btif_avk_get_init_delay
**
** Description      Returns init estimated delay
**
** Returns          uint32_t
*******************************************************************************/
uint32_t btif_avk_get_init_delay(RawAddress bd_addr);

/*******************************************************************************
**
** Function         btif_avk_split_get_delay
**
** Description      Returns init estimated delay for split a2dp sink.
**
** Returns          uint32_t
*******************************************************************************/
uint32_t btif_avk_split_get_delay(RawAddress bd_addr);
/*******************************************************************************
**
** Function         btif_avk_get_average_delay
**
** Description      Returns average of instantaneous delay values
**
** Returns          uint64_t
*******************************************************************************/
uint64_t btif_avk_get_average_delay(RawAddress bd_addr);

/*******************************************************************************
**
** Function         clear_delay_record
**
** Description      Clear delay record values
**
** Returns          void
*******************************************************************************/
void clear_delay_record(void);
#endif /* BTIF_AVK_H */
