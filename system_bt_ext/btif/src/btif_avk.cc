/******************************************************************************
 *  Copyright (C) 2017, The Linux Foundation. All rights reserved.
 *  Not a Contribution.
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted (subject to the limitations in the
 *  disclaimer below) provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

 *  NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 *  GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 *  HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 *  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/
/******************************************************************************
 *  Copyright (C) 2009-2016 Broadcom Corporation
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

#define LOG_TAG "btif_avk"

#include "btif_av.h"
#include "btif_avk.h"

#include <base/logging.h>
#include <string.h>

#include <hardware/bluetooth.h>
#include <hardware/bt_av.h>
#include <hardware/bt_rc_ext.h>

#include "audio_a2dp_hw/include/audio_a2dp_hw.h"
#include "bt_common.h"
#include "bt_utils.h"
#include "bta_api.h"
#include "btif_storage.h"
#include "btif_a2dp.h"
#include "btif_hf.h"
#include "btif_a2dp_audio_interface.h"
#include "btif_a2dp_control.h"
#include "btif_a2dp_sink.h"
#include "btif_avk_co.h"
#include "btif_profile_queue.h"
#include "btif_util.h"
#include "btu.h"
#include "osi/include/allocator.h"
#include "osi/include/osi.h"
#include "osi/include/properties.h"
#include "btif/include/btif_a2dp_source.h"
#include "device/include/interop.h"
#include "device/include/controller.h"
#include "btif_bat.h"
#include "bta/av/bta_av_int.h"
#include "device/include/device_iot_config.h"
#include "bta_avk_api.h"

extern bool isDevUiReq;
//static int reconfig_a2dp_param_id = 0;
//static int reconfig_a2dp_param_val = 0;

/*****************************************************************************
 *  Constants & Macros
 *****************************************************************************/
#define BTIF_AVK_SERVICE_NAME "Advanced Audio Sink"

#define BTIF_TIMEOUT_AV_OPEN_ON_RC_MS (2 * 1000)

/* Number of BTIF-AV control blocks */
/* Now supports Two AV connections. */
#define BTIF_AVK_NUM_CB       2
#define HANDLE_TO_INDEX(x) ((x & BTA_AVK_HNDL_MSK) - 1)
#define INVALID_INDEX        -1

typedef enum {
  BTIF_AVK_STATE_IDLE = 0x0,
  BTIF_AVK_STATE_OPENING,
  BTIF_AVK_STATE_OPENED,
  BTIF_AVK_STATE_STARTED,
  BTIF_AVK_STATE_CLOSING
} btif_avk_state_t;

/* Should not need dedicated suspend state as actual actions are no
   different than open state. Suspend flags are needed however to prevent
   media task from trying to restart stream during remote suspend or while
   we are in the process of a local suspend */

#define BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING 0x1
#define BTIF_AVK_FLAG_REMOTE_SUSPEND 0x2
#define BTIF_AVK_FLAG_PENDING_START 0x4
#define BTIF_AVK_FLAG_PENDING_STOP 0x8
#define BTIF_AVK_FLAG_PENDING_DISCONNECT 0x10
#define BTIF_TIMEOUT_AV_COLL_DETECTED_MS (2 * 1000)
#define BTIF_TIMEOUT_AV_COLL_DETECTED_MS_2 (4 * 1000)
#define BTIF_ERROR_SRV_AV_CP_NOT_SUPPORTED   705

/* Host role definitions */
#define HOST_ROLE_MASTER                   0x00
#define HOST_ROLE_SLAVE                    0x01
#define HOST_ROLE_UNKNOWN                  0xff

/* Default sink latency value while delay report is not supported by SNK */
#define BTIF_AVK_DEFAULT_SINK_LATENCY 0
#define BTIF_AVK_DEFAULT_MULTICAST_SINK_LATENCY 200

#define DELAY_RECORD_COUNT 100 /* No of packets considered for delay reporting calculation */
#define MISC_RENDERING_DELAY 50 /* miscellaneous rendering delay from SINK API's*/
#define QUICK_SINK_SHO_IND_PKT_CNT 165 /* when previous streaming device starts playing
                                          within 3 sec*/
#define MAX_ALLOWED_SINK_CONNECTIONS 2 /* Max Sink connections are restricted to 2 */

/* Param id for bitrate and bits per sample */
#define BITRATE_PARAM_ID 1
#define BITSPERSAMPLE_PARAM_ID 2

/* Delay in setting Audio Config if SETCONFIG is received before clearing previous config */
#define BTIF_DELAYED_UPDATE_DECODER_MS 150
/*****************************************************************************
 *  Local type definitions
 *****************************************************************************/

typedef struct {
  tBTA_AVK_HNDL bta_handle;
  RawAddress peer_bda;
  bool self_initiated_connection;
  btif_sm_handle_t sm_handle;
  uint8_t flags;
  tBTA_AVK_EDR edr;
  uint8_t peer_sep; /* sep type of peer device */
  std::vector<btav_a2dp_codec_config_t> codec_priorities;
  bool edr_3mbps;
  bool dual_handoff;
  bool current_playing;
  btif_sm_state_t state;
  int service;
  bool is_slave;
  bool is_device_playing;
  bool reconfig_pending; /* flag used for codec switch */
  tBTA_AVK_LATENCY sink_latency;
  bool remote_started;
  bool is_suspend_for_remote_start;
#if (TWS_ENABLED == TRUE)
  bool tws_device;
  bool offload_state;
#endif
  bool avdt_sync; /* for AVDT1.3 delay reporting */
  uint16_t codec_latency;
  uint16_t aptx_mode;
  btif_sm_event_t reconfig_event;
  tBTA_AVK reconfig_data;
  struct alarm_t *remote_start_alarm;
} btif_avk_cb_t;

typedef struct {
  RawAddress* target_bda;
  uint16_t uuid;
} btif_avk_connect_req_t;

typedef struct {
  int sample_rate;
  int channel_count;
  RawAddress peer_bd;
} btif_avk_sink_config_req_t;

/*****************************************************************************
 *  Static variables
 *****************************************************************************/
static btav_sink_callbacks_t* bt_avk_sink_callbacks = NULL;
static btif_avk_cb_t btif_avk_cb[BTIF_AVK_NUM_CB] = {
    { 0, {{0}}, false, 0, 0, 0, 0, std::vector<btav_a2dp_codec_config_t>(), false,
    false, false, BTIF_AVK_STATE_IDLE, BTA_A2DP_SINK_SERVICE_ID,
    false, false, false, 0, false, false
#if (TWS_ENABLED == TRUE)
    , false, false
#endif
    , false
    , 0, 0x1000, 0, {0}, NULL,},
    { 0, {{0}}, false, 0, 0, 0, 0, std::vector<btav_a2dp_codec_config_t>(), false,
    false, false, BTIF_AVK_STATE_IDLE, BTA_A2DP_SINK_SERVICE_ID,
    false, false, false, 0, false, false
#if (TWS_ENABLED == TRUE)
    , false, false
#endif
    , false
    , 0, 0x1000, 0, {0}, NULL,},
};

static alarm_t* avk_open_on_rc_timer = NULL;
static btif_sm_event_t idle_rc_event;
//static tBTA_AVK idle_rc_data;
int btif_max_avk_clients = 1;
static bool enable_multicast = false;
//static bool multicast_disabled = false;
//static RawAddress retry_bda;
//static RawAddress codec_bda = {};
//static int conn_retry_count = 1;
//static alarm_t *av_coll_detected_timer = NULL;
static bool isA2dpSink = false;
static bool codec_config_update_enabled = false;
extern bool is_codec_config_dump;
extern alarm_t* config_alarm;

/*SPLITA2DP */
extern bool bt_split_a2dp_enabled;
extern bool reconfig_a2dp;
extern bool btif_a2dp_audio_if_init;
extern bool audio_start_awaited;
extern bool enc_update_in_progress;
extern bool is_block_hal_start;
/*SPLITA2DP */
/* both interface and media task needs to be ready to alloc incoming request */
#define CHECK_BTAV_INIT()                                                    \
  do {                                                                       \
    if (  (bt_avk_sink_callbacks == NULL) || \
        (btif_avk_cb[0].sm_handle == NULL)) {                                    \
      BTIF_TRACE_WARNING("%s: BTAV not initialized", __func__);              \
      return BT_STATUS_NOT_READY;                                            \
    }                                                                        \
  } while (0)

/* Helper macro to avoid code duplication in the state machine handlers */
#define CHECK_RC_EVENT(e, d)       \
  case BTA_AVK_RC_BROWSE_OPEN_EVT:  \
  case BTA_AVK_RC_CLOSE_EVT:        \
  case BTA_AVK_RC_BROWSE_CLOSE_EVT: \
  case BTA_AVK_REMOTE_CMD_EVT:      \
  case BTA_AVK_VENDOR_CMD_EVT:      \
  case BTA_AVK_META_MSG_EVT:        \
  case BTA_AVK_RC_FEAT_EVT:         \
  case BTA_AVK_REMOTE_RSP_EVT: {    \
    btif_avk_rc_handler(e, d);         \
  } break;

void btif_avk_flow_spec_cmd(int index, int bitrate);

extern int64_t average_delay;
static int64_t delay_record[DELAY_RECORD_COUNT] = {0}; /* store latest packets delay */
static int delay_record_idx = 0;

static bool btif_avk_state_idle_handler(btif_sm_event_t event, void* data, int idx);
static bool btif_avk_state_opening_handler(btif_sm_event_t event, void* data, int idx);
static bool btif_avk_state_opened_handler(btif_sm_event_t event, void* data, int idx);
static bool btif_avk_state_started_handler(btif_sm_event_t event, void* data, int idx);
static bool btif_avk_state_closing_handler(btif_sm_event_t event, void* data, int idx);

static bool btif_avk_get_valid_idx(int idx);
int btif_avk_idx_by_bdaddr(RawAddress *bd_addr);
static int btif_avk_get_valid_idx_for_rc_events(RawAddress bd_addr, int rc_handle);
static int btif_avk_get_conn_state_of_device(RawAddress address);
static bt_status_t connect_int(RawAddress *bd_addr, uint16_t uuid);
static void btif_avk_check_rc_connection_priority(void *p_data);
static bt_status_t connect_int(RawAddress* bd_addr, uint16_t uuid);
int btif_avk_get_is_remote_started_idx();
bool btif_avk_is_state_opened(int i);
static void btif_avk_reset_remote_started_flag();
extern void btif_a2dp_update_sink_latency_change();
static int other_device_media_packet_count = 0;


#if (TWS_ENABLED == TRUE)
bool btif_avk_is_tws_device_playing(int index);
bool btif_avk_is_tws_suspend_triggered(int index);
bool btif_avk_is_tws_enabled_for_dev(const RawAddress& rc_addr);
bool btif_avk_is_tws_connected(void);
bool btif_avk_current_device_is_tws(void);
bool btif_avk_is_idx_tws_device(int index);
int btif_avk_get_tws_pair_idx(int index);
bool btif_avk_is_tws_enable_monocfg(void);
#else
#define btif_avk_is_tws_device_playing() 0
#define btif_avk_is_tws_suspend_triggered() 0
#define btif_avk_is_tws_enabled_for_dev() 0
#define btif_avk_is_tws_connected() 0
#define btif_avk_current_device_is_tws() 0
#define btif_avk_is_idx_tws_device() 0
#define btif_avk_get_tws_pair_idx() 0
#define btif_avk_is_tws_enable_monocfg() 0
#endif
#ifdef AVK_BACKPORT
void btif_av_request_audio_focus(bool enable);
#endif

static const btif_sm_handler_t btif_avk_state_handlers[] = {
    btif_avk_state_idle_handler, btif_avk_state_opening_handler,
    btif_avk_state_opened_handler, btif_avk_state_started_handler,
    btif_avk_state_closing_handler};

static void btif_avk_event_free_data(btif_sm_event_t event, void* p_data);

/*************************************************************************
 * Extern functions
 ************************************************************************/
extern void btif_avk_a2dp_on_suspended(tBTA_AVK_SUSPEND* p_av_suspend);
extern void btif_avk_rc_handler(tBTA_AV_EVT event, tBTA_AVK* p_data);
extern bool btif_avk_rc_get_connected_peer(RawAddress* peer_addr);
extern uint8_t btif_avk_rc_get_connected_peer_handle(const RawAddress& peer_addr);
extern RawAddress btif_avk_rc_get_connected_peer_address(uint8_t handle);
extern void btif_avk_rc_check_handle_pending_play(const RawAddress& peer_addr,
                                              bool bSendToApp);
extern void btif_avk_rc_get_playing_device(RawAddress *address);
extern void btif_avk_rc_clear_playing_state(bool play);
extern void btif_avk_rc_clear_priority(RawAddress address);
extern void btif_avk_rc_send_pause_command(RawAddress bda);
extern uint16_t btif_dm_get_br_edr_links();
extern uint16_t btif_dm_get_le_links();
extern void btif_a2dp_on_idle();
extern void btif_a2dp_sink_on_idle();
extern fixed_queue_t* btu_general_alarm_queue;
extern void btif_media_send_reset_vendor_state();
extern tBTIF_A2DP_SOURCE_VSC btif_a2dp_src_vsc;
extern uint8_t* bta_avk_co_get_peer_codec_info(uint8_t hdl);
extern bool bta_avk_is_avdt_sync(uint16_t handle);

/*****************************************************************************
 * Local helper functions
 *****************************************************************************/
bool btif_avk_is_connected_on_other_idx(int current_index);
bool btif_avk_is_playing_on_other_idx(int current_index);
//tBTA_AVK_HNDL btif_av_get_playing_device_hdl(); //TODO
tBTA_AVK_HNDL btif_avk_get_av_hdl_from_idx(int idx);
int btif_avk_get_other_connected_idx(int current_index);
void btif_avk_reset_reconfig_flag();
bool btif_avk_is_device_disconnecting();
tBTA_AVK_HNDL btif_avk_get_reconfig_dev_hndl();
void btif_avk_reset_codec_reconfig_flag(RawAddress address);
void btif_avk_reinit_audio_interface();
bool btif_avk_is_suspend_stop_pending_ack();


const char* dump_avk_sm_state_name(btif_avk_state_t state) {
  switch (state) {
    CASE_RETURN_STR(BTIF_AVK_STATE_IDLE)
    CASE_RETURN_STR(BTIF_AVK_STATE_OPENING)
    CASE_RETURN_STR(BTIF_AVK_STATE_OPENED)
    CASE_RETURN_STR(BTIF_AVK_STATE_STARTED)
    CASE_RETURN_STR(BTIF_AVK_STATE_CLOSING)
    default:
      return "UNKNOWN_STATE";
  }
}

const char* dump_avk_sm_event_name(btif_avk_sm_event_t event) {
  switch ((int)event) {
    CASE_RETURN_STR(BTA_AVK_ENABLE_EVT)
    CASE_RETURN_STR(BTA_AVK_REGISTER_EVT)
    CASE_RETURN_STR(BTA_AVK_OPEN_EVT)
    CASE_RETURN_STR(BTA_AVK_CLOSE_EVT)
    CASE_RETURN_STR(BTA_AVK_START_EVT)
    CASE_RETURN_STR(BTA_AVK_STOP_EVT)
    CASE_RETURN_STR(BTA_AVK_PROTECT_REQ_EVT)
    CASE_RETURN_STR(BTA_AVK_PROTECT_RSP_EVT)
    CASE_RETURN_STR(BTA_AVK_RC_OPEN_EVT)
    CASE_RETURN_STR(BTA_AVK_RC_CLOSE_EVT)
    CASE_RETURN_STR(BTA_AVK_RC_BROWSE_OPEN_EVT)
    CASE_RETURN_STR(BTA_AVK_RC_BROWSE_CLOSE_EVT)
    CASE_RETURN_STR(BTA_AVK_REMOTE_CMD_EVT)
    CASE_RETURN_STR(BTA_AVK_REMOTE_RSP_EVT)
    CASE_RETURN_STR(BTA_AVK_VENDOR_CMD_EVT)
    CASE_RETURN_STR(BTA_AVK_VENDOR_RSP_EVT)
    CASE_RETURN_STR(BTA_AVK_RECONFIG_EVT)
    CASE_RETURN_STR(BTA_AVK_SUSPEND_EVT)
    CASE_RETURN_STR(BTA_AVK_PENDING_EVT)
    CASE_RETURN_STR(BTA_AVK_META_MSG_EVT)
    CASE_RETURN_STR(BTA_AVK_REJECT_EVT)
    CASE_RETURN_STR(BTA_AVK_RC_FEAT_EVT)
    CASE_RETURN_STR(BTA_AVK_DELAY_REPORT_EVT)
    CASE_RETURN_STR(BTIF_SM_ENTER_EVT)
    CASE_RETURN_STR(BTIF_SM_EXIT_EVT)
    CASE_RETURN_STR(BTIF_AVK_CONNECT_REQ_EVT)
    CASE_RETURN_STR(BTIF_AVK_DISCONNECT_REQ_EVT)
    CASE_RETURN_STR(BTIF_AVK_START_STREAM_REQ_EVT)
    CASE_RETURN_STR(BTIF_AVK_STOP_STREAM_REQ_EVT)
    CASE_RETURN_STR(BTIF_AVK_SUSPEND_STREAM_REQ_EVT)
    CASE_RETURN_STR(BTIF_AVK_SOURCE_CONFIG_REQ_EVT)
    CASE_RETURN_STR(BTIF_AVK_SOURCE_CONFIG_UPDATED_EVT)
    CASE_RETURN_STR(BTIF_AVK_REMOTE_SUSPEND_STREAM_REQ_EVT)
    CASE_RETURN_STR(BTIF_AVK_SINK_CONFIG_REQ_EVT)
    CASE_RETURN_STR(BTIF_AVK_SETUP_CODEC_REQ_EVT)
    CASE_RETURN_STR(BTA_AVK_SINK_MEDIA_CFG_EVT)
    default:
      return "UNKNOWN_EVENT";
  }
}

/****************************************************************************
 *  Local helper functions
 ****************************************************************************/
/*******************************************************************************
 *
 * Function         btif_initiate_avk_open_timer_timeout
 *
 * Description      Timer to trigger AV open if the remote headset establishes
 *                  RC connection w/o AV connection. The timer is needed to IOP
 *                  with headsets that do establish AV after RC connection.
 *
 * Returns          void
 *
 ******************************************************************************/
static void btif_initiate_avk_open_timer_timeout(UNUSED_ATTR void* data) {
  RawAddress peer_addr;
  btif_avk_connect_req_t connect_req;

  memset(&connect_req, 0, sizeof(btif_avk_connect_req_t));
  /* is there at least one RC connection - There should be */
  if (btif_avk_rc_get_connected_peer(&peer_addr)) {
    /* Check if this peer_addr is same as currently connected AV*/
    if (btif_avk_get_conn_state_of_device(peer_addr) == BTIF_AVK_STATE_OPENED) {
      BTIF_TRACE_DEBUG("AV is already connected");
    } else {
      uint8_t rc_handle;
      int index;
      /* Multicast: Check if AV slot is available for connection
       * If not available, AV got connected to different devices.
       * Disconnect this RC connection without AV connection.
       */
      rc_handle = btif_avk_rc_get_connected_peer_handle(peer_addr);
      index = btif_avk_get_valid_idx_for_rc_events(peer_addr, rc_handle);
      if (index >= btif_max_avk_clients) {
          BTIF_TRACE_ERROR("%s No slot free for AV connection, back off",
                            __func__);
          return;
      }
      BTIF_TRACE_DEBUG("%s Issuing connect to the remote RC peer", __func__);
    /* In case of AVRCP connection request, we will initiate SRC connection */
    connect_req.target_bda = &peer_addr;
    connect_req.uuid = UUID_SERVCLASS_AUDIO_SINK;
    btif_avk_dispatch_sm_event(BTIF_AVK_CONNECT_REQ_EVT, (char*)&connect_req,
                           sizeof(connect_req));
    }
  } else {
    BTIF_TRACE_ERROR("%s: No connected RC peers", __func__);
  }
}

/*****************************************************************************
 *  Static functions
 *****************************************************************************/

/*******************************************************************************
 *
 * Function         btif_report_connection_state
 *
 * Description      Updates the components via the callbacks about the
 *                  connection state of a2dp connection.
 *
 * Returns          None
 *
 ******************************************************************************/
static void btif_report_connection_state(btav_connection_state_t state,
                                         RawAddress* bd_addr) {
  if (bt_avk_sink_callbacks != NULL) {
    HAL_CBACK(bt_avk_sink_callbacks, connection_state_cb, *bd_addr, state);
  }
}

static void btif_report_connection_state_to_ba(btav_connection_state_t state) {
  // send updates to BA as well
  if (state == BTAV_CONNECTION_STATE_DISCONNECTED)
  {
      ba_send_message(BTIF_BA_BT_A2DP_DISC_EVT, 0, NULL, true);
  }
}
/*******************************************************************************
 *
 * Function         btif_report_audio_state
 *
 * Description      Updates the components via the callbacks about the audio
 *                  state of a2dp connection. The state is updated when either
 *                  the remote ends starts streaming (started state) or whenever
 *                  it transitions out of started state (to opened or streaming)
 *                  state.
 *
 * Returns          None
 *
 ******************************************************************************/
static void btif_report_audio_state(btav_audio_state_t state,
                                    RawAddress* bd_addr) {
  if (bt_avk_sink_callbacks != NULL) {
    HAL_CBACK(bt_avk_sink_callbacks, audio_state_cb, *bd_addr, state);
  }
}

static void btif_report_audio_state_to_ba(btav_audio_state_t state) {
  if (state == BTAV_AUDIO_STATE_STARTED)
  {
      ba_send_message(BTIF_BA_BT_A2DP_STARTED_EVT, 0, NULL, true);
  }
  if ((state == BTAV_AUDIO_STATE_REMOTE_SUSPEND) ||
      (state == BTAV_AUDIO_STATE_STOPPED))
  {
      ba_send_message(BTIF_BA_BT_A2DP_PAUSED_EVT, 0, NULL, true);
  }
}







/*****************************************************************************
 *
 * Function     btif_avk_state_idle_handler
 *
 * Description  State managing disconnected AV link
 *
 * Returns      true if event was processed, false otherwise
 *
 ******************************************************************************/

static bool btif_avk_state_idle_handler(btif_sm_event_t event, void* p_data, int index) {
  bool other_device_connected = false;
  BTIF_TRACE_IMP("%s event:%s flags %x on index %x", __func__,
                   dump_avk_sm_event_name((btif_avk_sm_event_t)event),
                   btif_avk_cb[index].flags, index);

  switch (event) {
    case BTIF_SM_ENTER_EVT:
      /* clear the peer_bda */
      BTIF_TRACE_EVENT("%s, IDLE state for index: %d", __func__, index);
      memset(&btif_avk_cb[index].peer_bda, 0, sizeof(RawAddress)); //TODO
      btif_avk_cb[index].flags = 0;
      btif_avk_cb[index].edr_3mbps = false;
      btif_avk_cb[index].edr = 0;
      btif_avk_cb[index].current_playing = false;
      btif_avk_cb[index].is_slave = false;
      btif_avk_cb[index].is_device_playing = false;
      btif_avk_cb[index].reconfig_pending = false;
      btif_avk_cb[index].sink_latency = 0;
      btif_avk_cb[index].remote_started = false;
      btif_avk_cb[index].remote_start_alarm = NULL;
      btif_avk_cb[index].is_suspend_for_remote_start = false;
#if (TWS_ENABLED == TRUE)
      BTIF_TRACE_EVENT("reset tws_device flag in IDLE state");
      btif_avk_cb[index].tws_device = false;
      btif_avk_cb[index].offload_state = false;
#endif
      btif_avk_cb[index].codec_latency = 0;
      btif_avk_cb[index].reconfig_event = 0;
      memset(&btif_avk_cb[index].reconfig_data, 0, sizeof(tBTA_AVK));
      for (int i = 0; i < btif_max_avk_clients; i++)
        btif_avk_cb[i].dual_handoff = false;
      isA2dpSink = true;
      /* This API will be called twice at initialization
      ** Idle can be moved when device is disconnected too.
      ** Take care of other connected device here.*/
      for (int i = 0; i < btif_max_avk_clients; i++) {
        if ((i != index) && btif_avk_get_valid_idx(i)) {
          other_device_connected = true;
          break;
        }
      }
      if (other_device_connected == false) {
        BTIF_TRACE_EVENT("reset A2dp states in IDLE ");
        bta_avk_co_init(btif_avk_cb[index].codec_priorities);
        btif_a2dp_sink_on_idle();
      } else {
        //There is another AV connection, update current playin
        BTIF_TRACE_EVENT("idle state for index %d init_co", index);
        bta_avk_co_peer_init(btif_avk_cb[index].codec_priorities, index);
      }
      break;

    case BTIF_SM_EXIT_EVT:
      if (codec_config_update_enabled == false) {
        codec_config_update_enabled = true;
        BTIF_TRACE_IMP("%s: Codec config update enabled changed to true", __func__);
      }
      break;

    case BTA_AVK_ENABLE_EVT:
      BTIF_TRACE_EVENT("AV is enabled now for index: %d", index);
      break;

    case BTA_AVK_REGISTER_EVT:
      BTIF_TRACE_EVENT("The AV Handle:%d", ((tBTA_AVK*)p_data)->registr.hndl);
      btif_avk_cb[index].bta_handle = ((tBTA_AVK*)p_data)->registr.hndl;
      break;

    case BTIF_AVK_CONNECT_REQ_EVT: {
        btif_avk_connect_req_t* connect_req_p = (btif_avk_connect_req_t*)p_data;
        btif_avk_cb[index].peer_bda = *connect_req_p->target_bda;
        BTA_AvkOpen(btif_avk_cb[index].peer_bda, btif_avk_cb[index].bta_handle, true,
                   BTA_SEC_AUTHENTICATE, connect_req_p->uuid);
#if (BT_IOT_LOGGING_ENABLED == TRUE)
        device_iot_config_addr_set_int(btif_avk_cb[index].peer_bda, IOT_CONF_KEY_A2DP_ROLE,
            (connect_req_p->uuid == UUID_SERVCLASS_AUDIO_SOURCE) ? IOT_CONF_VAL_A2DP_ROLE_SINK :
            IOT_CONF_VAL_A2DP_ROLE_SOURCE);
        device_iot_config_addr_int_add_one(btif_avk_cb[index].peer_bda,
            IOT_CONF_KEY_A2DP_CONN_COUNT);
#endif
#if (TWS_ENABLED == TRUE)
      BTIF_TRACE_EVENT("update tws device status");
      btif_avk_cb[index].tws_device = BTM_SecIsTwsPlusDev(btif_avk_cb[index].peer_bda.address);
      BTA_AvkUpdateTWSDevice(btif_avk_cb[index].tws_device, btif_avk_cb[index].bta_handle);
#endif
      btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_OPENING);
      } break;

    case BTA_AVK_PENDING_EVT:
    case BTA_AVK_RC_OPEN_EVT:
      /* IOP_FIX: Jabra 620 only does RC open without AV open whenever it
       * connects. So
       * as per the AV WP, an AVRC connection cannot exist without an AV
       * connection. Therefore,
       * we initiate an AV connection if an RC_OPEN_EVT is received when we are
       * in AV_CLOSED state.
       * We initiate the AV connection after a small 3s timeout to avoid any
       * collisions from the
       * headsets, as some headsets initiate the AVRC connection first and then
       * immediately initiate the AV connection
       *
       * TODO: We may need to do this only on an AVRCP Play. FixMe
       */
      /* Check if connection allowed with this device
       * In Dual A2dp case, this event can come for both the headsets.
       * Reject second connection request as we are already checking
       * for device priority for first device and we cannot queue
       * incoming connections requests.
       */
      if (event == BTA_AVK_RC_OPEN_EVT) {
        if (((tBTA_AVK*)p_data)->rc_open.status == BTA_AVK_SUCCESS) {
            btif_avk_cb[index].peer_bda = ((tBTA_AVK*)p_data)->rc_open.peer_addr;
        } else {
          idle_rc_event = 0;
          return true;
        }
      } else
        btif_avk_cb[index].peer_bda = ((tBTA_AVK*)p_data)->pend.bd_addr;

      // Only for AVDTP connection request move to opening state
      if (event == BTA_AVK_PENDING_EVT) {
#if (BT_IOT_LOGGING_ENABLED == TRUE)
        device_iot_config_addr_int_add_one(btif_avk_cb[index].peer_bda,
            IOT_CONF_KEY_A2DP_CONN_COUNT);
#endif
        btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_OPENING);
      }

      if (bt_avk_sink_callbacks != NULL) {
        if (event == BTA_AVK_PENDING_EVT)
            BTA_AvkOpen(btif_avk_cb[index].peer_bda, btif_avk_cb[index].bta_handle,
               true, BTA_SEC_AUTHENTICATE, UUID_SERVCLASS_AUDIO_SINK);
        else if (event == BTA_AVK_RC_OPEN_EVT) {
          alarm_set_on_mloop(avk_open_on_rc_timer,
          BTIF_TIMEOUT_AV_OPEN_ON_RC_MS,
          btif_initiate_avk_open_timer_timeout, NULL);
          btif_avk_rc_handler(event, (tBTA_AVK *)p_data);
        }
      }
      break;

    case BTA_AVK_RC_BROWSE_OPEN_EVT:
      BTIF_TRACE_DEBUG("%s: BTA_AVK_RC_BROWSE_OPEN_EVT received", __func__);
      btif_avk_rc_handler(event, (tBTA_AVK*)p_data);
      break;

    /*
     * In case Signalling channel is not down
     * and remote started Streaming Procedure
     * we have to handle config and open event in
     * idle_state. We hit these scenarios while running
     * PTS test case for AVRCP Controller
     */
    case BTIF_AVK_SINK_CONFIG_REQ_EVT: {
      btif_avk_sink_config_req_t req;
      // copy to avoid alignment problems
      memcpy(&req, p_data, sizeof(req));

      BTIF_TRACE_WARNING(
          "%s: BTIF_AVK_SINK_CONFIG_REQ_EVT sample_rate=%d "
          "channel_count=%d",
          __func__, req.sample_rate, req.channel_count);
      if (bt_avk_sink_callbacks != NULL) {
        HAL_CBACK(bt_avk_sink_callbacks, audio_config_cb, req.peer_bd,
                  req.sample_rate, req.channel_count);
      }
    } break;

    case BTA_AVK_OPEN_EVT: {
      /* We get this event in Idle State if Signaling
       * channel is not closed, only Streaming channel was
       * closed earlier, and now only stream setup process is
       * initiated.
       */
      tBTA_AVK *p_bta_data = (tBTA_AVK*)p_data;
      btav_connection_state_t state;
      BTIF_TRACE_DEBUG("status:%d, edr 0x%x",p_bta_data->open.status,
                               p_bta_data->open.edr);

      if (p_bta_data->open.status == BTA_AVK_SUCCESS) {
        state = BTAV_CONNECTION_STATE_CONNECTED;
        btif_avk_cb[index].edr = p_bta_data->open.edr;
        if (p_bta_data->open.role == HOST_ROLE_SLAVE)
          btif_avk_cb[index].is_slave = true;
        btif_avk_cb[index].peer_sep = p_bta_data->open.sep;
#if (TWS_ENABLED == TRUE)
        BTIF_TRACE_DEBUG("is tws_device = %d",p_bta_data->open.tws_device);
        btif_avk_cb[index].tws_device = p_bta_data->open.tws_device;
#endif
        if (p_bta_data->open.edr & BTA_AVK_EDR_3MBPS) {
          BTIF_TRACE_DEBUG("remote supports 3 mbps");
          btif_avk_cb[index].edr_3mbps = true;
        }

        btif_avk_cb[index].peer_bda = ((tBTA_AVK*)p_data)->open.bd_addr;
      } else {
        BTIF_TRACE_WARNING("BTA_AVK_OPEN_EVT::FAILED status: %d",
                             p_bta_data->open.status );
        state = BTAV_CONNECTION_STATE_DISCONNECTED;
      }

      /* change state to open based on the status */
      if (p_bta_data->open.status == BTA_AVK_SUCCESS) {
        /* inform the application of the event */
        if (btif_avk_is_split_a2dp_enabled() && //TODO yanhuig avk init audio interface
          btif_a2dp_audio_if_init != true) {
          BTIF_TRACE_DEBUG("Got OPEN_EVT in IDLE state, init audio interface");
          btif_a2dp_audio_interface_init();
          btif_a2dp_audio_if_init = true;
        }
        btif_report_connection_state(state, &(btif_avk_cb[index].peer_bda));
        btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_OPENED);
        btif_report_connection_state_to_ba(state);
        /* BTIF AV State updated, now check
         * and update multicast state
         */
        if (btif_avk_cb[index].peer_sep == AVDT_TSEP_SRC) {
          /* if queued PLAY command,  send it now */
          btif_avk_rc_check_handle_pending_play(p_bta_data->open.bd_addr, false);
          /* Bring up AVRCP connection too */
          BTA_AvkOpenRc(btif_avk_cb[index].bta_handle);
        }
      }

      btif_avk_connect_req_t* connect_req_t = (btif_avk_connect_req_t*)p_data;
      connect_req_t->uuid = UUID_SERVCLASS_AUDIO_SINK;
      btif_queue_advance_by_uuid(connect_req_t->uuid, &(btif_avk_cb[index].peer_bda));
    } break;

    case BTA_AVK_REMOTE_CMD_EVT:
    case BTA_AVK_VENDOR_CMD_EVT:
    case BTA_AVK_META_MSG_EVT:
    case BTA_AVK_RC_FEAT_EVT:
    case BTA_AVK_REMOTE_RSP_EVT:
      btif_avk_rc_handler(event, (tBTA_AVK*)p_data);
      break;

    case BTA_AVK_RC_CLOSE_EVT:
      BTIF_TRACE_DEBUG("%s: BTA_AVK_RC_CLOSE_EVT: Stopping AV timer.", __func__);
      BTIF_TRACE_DEBUG("BTA_AVK_RC_CLOSE_EVT: clear peer_bda.address index : %d", index);
      btif_avk_cb[index].peer_bda =  RawAddress::kEmpty;
      BTIF_TRACE_DEBUG("BTA_AVK_RC_CLOSE_EVT: Stopping AV timer.");
      alarm_cancel(avk_open_on_rc_timer);
      btif_avk_rc_handler(event, (tBTA_AVK*)p_data);
      break;


    case BTA_AVK_SINK_MEDIA_CFG_EVT: {
      tBTA_AVK_SINK_MEDIA* p_data_cur = (tBTA_AVK_SINK_MEDIA *)p_data;
      btif_a2dp_sink_update_decoder((uint8_t*)(p_data_cur->avk_config.codec_info));
      } break;

    default:
      BTIF_TRACE_WARNING("%s: unhandled event=%s", __func__,
                         dump_avk_sm_event_name((btif_avk_sm_event_t)event));
      return false;
  }

  return true;
}
/*****************************************************************************
 *
 * Function        btif_avk_state_opening_handler
 *
 * Description     Intermediate state managing events during establishment
 *                 of avdtp channel
 *
 * Returns         true if event was processed, false otherwise
 *
 ******************************************************************************/

static bool btif_avk_state_opening_handler(btif_sm_event_t event, void* p_data,
                                          int index) {
  //RawAddress* bt_addr = nullptr;
  BTIF_TRACE_IMP("%s event:%s flags %x on index %x", __func__,
                   dump_avk_sm_event_name((btif_avk_sm_event_t)event),
                   btif_avk_cb[index].flags, index);

  switch (event) {
    case BTIF_SM_ENTER_EVT:
      //When uncheck media audio from settings UI and try to connect from remote,
      //a2dp would fail. Then check the media audio from UI, then due to peer codec info
      //null, so it will not go for A2dp connection. So reinit peer codecs unconditionally.
      BTIF_TRACE_DEBUG("%s: initialize peer codecs, unconditionally.", __func__);
      bta_avk_co_peer_init(btif_avk_cb[index].codec_priorities, index);
      /* inform the application that we are entering connecting state */
      if (bt_avk_sink_callbacks != NULL)
        HAL_CBACK(bt_avk_sink_callbacks, connection_state_cb,
                  btif_avk_cb[index].peer_bda, BTAV_CONNECTION_STATE_CONNECTING);
      break;

    case BTIF_SM_EXIT_EVT:
      break;

    case BTA_AVK_REJECT_EVT:
      BTIF_TRACE_WARNING("%s: Received BTA_AVK_REJECT_EVT", __func__);
      btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
                                   &(btif_avk_cb[index].peer_bda));
      btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_IDLE);
#if (BT_IOT_LOGGING_ENABLED == TRUE)
      device_iot_config_addr_int_add_one(btif_avk_cb[index].peer_bda,
          IOT_CONF_KEY_A2DP_CONN_FAIL_COUNT);
#endif
      break;

    case BTA_AVK_OPEN_EVT: {
      tBTA_AVK* p_bta_data = (tBTA_AVK*)p_data;
      btav_connection_state_t state;
      btif_sm_state_t av_state;
      BTIF_TRACE_DEBUG("status:%d, edr 0x%x, role: 0x%x", p_bta_data->open.status,
                       p_bta_data->open.edr, p_bta_data->open.role);

      if (p_bta_data->open.status == BTA_AVK_SUCCESS) {
        state = BTAV_CONNECTION_STATE_CONNECTED;
        av_state = BTIF_AVK_STATE_OPENED;
/* SPLITA2DP */
        if (btif_a2dp_audio_if_init != true) {
          if (btif_avk_is_split_a2dp_enabled()) {//TODO yanhuig if audio interface only init with split a2dp, should we remove it here?
            BTIF_TRACE_DEBUG("Split a2dp enabled:initialize interface ");
            btif_a2dp_audio_if_init = true;
            btif_a2dp_audio_interface_init();
          } else {
            BTIF_TRACE_DEBUG("Split a2dp not enabled");
          }
        } else {
          BTIF_TRACE_DEBUG("audio interface is already initialized");
        }
/* SPLITA2DP */
        btif_avk_cb[index].edr = p_bta_data->open.edr;
        if (p_bta_data->open.role == HOST_ROLE_SLAVE)
          btif_avk_cb[index].is_slave = true;
        btif_avk_cb[index].peer_sep = p_bta_data->open.sep;
#if (TWS_ENABLED == TRUE)
        BTIF_TRACE_DEBUG("is tws_device = %d",p_bta_data->open.tws_device);
        btif_avk_cb[index].tws_device = p_bta_data->open.tws_device;
#endif
        if (p_bta_data->open.edr & BTA_AVK_EDR_3MBPS) {
          BTIF_TRACE_DEBUG("remote supports 3 mbps");
          btif_avk_cb[index].edr_3mbps = true;
        }
        btif_avk_cb[index].avdt_sync = bta_avk_is_avdt_sync(btif_avk_cb[index].bta_handle);

        if (btif_avk_cb[index].avdt_sync) {
          BTIF_TRACE_DEBUG("avdt_sync TRUE");
        } else {
          BTIF_TRACE_DEBUG("avdt_sync FALSE");
        }
      } else {
        BTIF_TRACE_WARNING("%s: BTA_AVK_OPEN_EVT::FAILED status: %d", __func__,
                           p_bta_data->open.status);

        /* Multicast: Check if connected to AVRC only device
         * disconnect when Dual A2DP/Multicast is supported.
         */
        RawAddress peer_addr;
        uint8_t peer_handle = BTRC_HANDLE_NONE;
        if (btif_avk_rc_get_connected_peer(&peer_addr) &&
            (btif_avk_cb[index].peer_bda == peer_addr)) {
          /* Do not disconnect AVRCP connection if A2DP
           * connection failed due to SDP failure since remote
           * may not support A2DP. In such case we will keep
           * AVRCP only connection.
           */
          if (p_bta_data->open.status != BTA_AVK_FAIL_SDP) {
            BTIF_TRACE_WARNING("%s: Disconnecting AVRCP: peer_addr=%s", __func__,
                             peer_addr.ToString().c_str());
            peer_handle = btif_avk_rc_get_connected_peer_handle(peer_addr);
            if (peer_handle != BTRC_HANDLE_NONE) {
              BTA_AvkCloseRc(peer_handle);
            }
          } else {
            BTIF_TRACE_WARNING("Keep AVRCP only connection");
          }
        }
        state = BTAV_CONNECTION_STATE_DISCONNECTED;
        av_state = BTIF_AVK_STATE_IDLE;
#if (BT_IOT_LOGGING_ENABLED == TRUE)
        device_iot_config_addr_int_add_one(btif_avk_cb[index].peer_bda,
            IOT_CONF_KEY_A2DP_CONN_FAIL_COUNT);
#endif
      }
      /* inform the application of the event */
      btif_report_connection_state(state, &(btif_avk_cb[index].peer_bda));
      /* change state to open/idle based on the status */
      btif_sm_change_state(btif_avk_cb[index].sm_handle, av_state);
      btif_report_connection_state_to_ba(state);
      /* Check if the other connected AV is playing. If YES, trigger DUAL Handoff */
      if (p_bta_data->open.status == BTA_AVK_SUCCESS) {
        /* BTIF AV State updated, now check
         * and update multicast state
         */
        if (btif_avk_cb[index].peer_sep == AVDT_TSEP_SRC) {
          /* if queued PLAY command,  send it now */
          btif_avk_rc_check_handle_pending_play(p_bta_data->open.bd_addr, false);
          /* Bring up AVRCP connection too */
          BTA_AvkOpenRc(btif_avk_cb[index].bta_handle);
        }
      }
        btif_avk_connect_req_t* connect_req_t = (btif_avk_connect_req_t*)p_data;
        if (bt_avk_sink_callbacks != NULL)
            connect_req_t->uuid = UUID_SERVCLASS_AUDIO_SINK;
        btif_queue_advance_by_uuid(connect_req_t->uuid, &(btif_avk_cb[index].peer_bda));
    } break;



    case BTIF_AVK_SINK_CONFIG_REQ_EVT: {
      btif_avk_sink_config_req_t req;
      // copy to avoid alignment problems
      memcpy(&req, p_data, sizeof(req));

      BTIF_TRACE_WARNING("BTIF_AVK_SINK_CONFIG_REQ_EVT %d %d", req.sample_rate,
                         req.channel_count);
      if (btif_avk_cb[index].peer_sep == AVDT_TSEP_SRC &&
          bt_avk_sink_callbacks != NULL) {
        HAL_CBACK(bt_avk_sink_callbacks, audio_config_cb, btif_avk_cb[index].peer_bda,
                  req.sample_rate, req.channel_count);
      }
    } break;

    case BTIF_AVK_CONNECT_REQ_EVT: {
      // Check for device, if same device which moved to opening then ignore
      // callback
      btif_avk_connect_req_t* connect_req_p = (btif_avk_connect_req_t*)p_data;
      if (memcmp(((btif_avk_connect_req_t*)p_data)->target_bda,
                 &(btif_avk_cb[index].peer_bda), sizeof(btif_avk_cb[index].peer_bda)) == 0) {
        BTIF_TRACE_DEBUG(
            "%s: Same device moved to Opening state,ignore Connect Req",
            __func__);
      } else {
          RawAddress& target_bda = *connect_req_p->target_bda;
        BTIF_TRACE_WARNING(
            "%s: device %s is already connecting, reject Connect request to %s",
            __func__, btif_avk_cb[index].peer_bda.ToString().c_str(),
            target_bda.ToString().c_str());
        btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
                                     &target_bda);
        btif_report_connection_state_to_ba(BTAV_CONNECTION_STATE_DISCONNECTED);
      }
      // Ignore all connection request if we are already opening
      if (bt_avk_sink_callbacks != NULL)
          connect_req_p->uuid = UUID_SERVCLASS_AUDIO_SINK;
      btif_queue_advance_by_uuid(connect_req_p->uuid, &(btif_avk_cb[index].peer_bda));
    } break;

    case BTA_AVK_PENDING_EVT: {
      // Check for device, if same device which moved to opening then ignore
      // callback
      if (((tBTA_AVK*)p_data)->pend.bd_addr == btif_avk_cb[index].peer_bda) {
        BTIF_TRACE_DEBUG(
            "%s: Same device moved to Opening state,ignore Pending Req",
            __func__);
        break;
      } else {
          const RawAddress& bd_addr = ((tBTA_AVK*)p_data)->pend.bd_addr;
        BTIF_TRACE_WARNING(
            "%s: device %s is already connecting, reject incoming request "
            "from %s",
            __func__, btif_avk_cb[index].peer_bda.ToString().c_str(),
            bd_addr.ToString().c_str());
        BTA_AvkDisconnect(bd_addr);
      }
    } break;


    case BTA_AVK_CLOSE_EVT: {
      /* avdtp link is closed. Check if any other device is playing
       * and this is not the one
       */
      if (!btif_avk_is_playing())
        btif_a2dp_sink_on_stopped(NULL);
      btif_avk_cb[index].avdt_sync = FALSE;
      /* inform the application that we are disconnected */
      btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
         &(btif_avk_cb[index].peer_bda));
      btif_avk_connect_req_t* connect_req_t = (btif_avk_connect_req_t*)p_data;
      if (bt_avk_sink_callbacks != NULL)
          connect_req_t->uuid = UUID_SERVCLASS_AUDIO_SINK;
      btif_queue_advance_by_uuid(connect_req_t->uuid, &(btif_avk_cb[index].peer_bda));
      btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_IDLE);
      btif_report_connection_state_to_ba(BTAV_CONNECTION_STATE_DISCONNECTED);
#if (BT_IOT_LOGGING_ENABLED == TRUE)
      device_iot_config_addr_int_add_one(btif_avk_cb[index].peer_bda,
          IOT_CONF_KEY_A2DP_CONN_FAIL_COUNT);
#endif
      } break;

    case BTIF_AVK_DISCONNECT_REQ_EVT: {
       uint8_t peer_handle = BTRC_HANDLE_NONE;
       btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
           &(btif_avk_cb[index].peer_bda));
       if (!btif_avk_cb[index].peer_bda.IsEmpty())
         peer_handle = btif_avk_rc_get_connected_peer_handle(btif_avk_cb[index].peer_bda);

       if (peer_handle != BTRC_HANDLE_NONE) {
         BTIF_TRACE_WARNING("%s: RC connected to %s, disc RC too since AV is being aborted",
                 __func__, btif_avk_cb[index].peer_bda.ToString().c_str());
         BTA_AvkCloseRc(peer_handle);
       }
       BTA_AvkClose(btif_avk_cb[index].bta_handle);
       btif_queue_advance();
       btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_IDLE);
       btif_report_connection_state_to_ba(BTAV_CONNECTION_STATE_DISCONNECTED);
       } break;

    case BTA_AVK_RC_OPEN_EVT:
       btif_avk_rc_handler(event, (tBTA_AVK*)p_data);;
       break;

    case BTA_AVK_SINK_MEDIA_CFG_EVT: {
      tBTA_AVK_SINK_MEDIA* p_data_cur = (tBTA_AVK_SINK_MEDIA *)p_data;
      btif_a2dp_sink_update_decoder((uint8_t*)(p_data_cur->avk_config.codec_info));
      } break;


      CHECK_RC_EVENT(event, (tBTA_AVK*)p_data);

    default:
      BTIF_TRACE_WARNING("%s: unhandled event=%s", __func__,
                         dump_avk_sm_event_name((btif_avk_sm_event_t)event));
      return false;
  }
  return true;
}

/*****************************************************************************
 *
 * Function        btif_avk_state_closing_handler
 *
 * Description     Intermediate state managing events during closing
 *                 of avdtp channel
 *
 * Returns         true if event was processed, false otherwise
 *
 ******************************************************************************/

static bool btif_avk_state_closing_handler(btif_sm_event_t event, void* p_data, int index) {

  BTIF_TRACE_IMP("%s event:%s flags %x and index %x", __func__,
                   dump_avk_sm_event_name((btif_avk_sm_event_t)event),
                   btif_avk_cb[index].flags, index);

  switch (event) {
    case BTIF_SM_ENTER_EVT:
      if (btif_avk_cb[index].peer_sep == AVDT_TSEP_SRC && btif_avk_cb[index].current_playing)
        btif_a2dp_sink_set_rx_flush(true);
      break;

    case BTA_AVK_STOP_EVT:
    case BTIF_AVK_STOP_STREAM_REQ_EVT:
      if (btif_avk_cb[index].peer_sep == AVDT_TSEP_SRC) {
        btif_a2dp_sink_set_rx_flush(true);
        btif_a2dp_sink_on_stopped(NULL);
      }
      break;

    case BTIF_SM_EXIT_EVT:
      break;



    case BTA_AVK_CLOSE_EVT:
      /* inform the application that we are disconnecting */
      btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
                                   &(btif_avk_cb[index].peer_bda));

      btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_IDLE);
      btif_report_connection_state_to_ba(BTAV_CONNECTION_STATE_DISCONNECTED);
      break;

    /* Handle the RC_CLOSE event for the cleanup */
    case BTA_AVK_RC_CLOSE_EVT:
      btif_avk_rc_handler(event, (tBTA_AVK*)p_data);
      break;

    /* Handle the RC_BROWSE_CLOSE event for tetsing*/
    case BTA_AVK_RC_BROWSE_CLOSE_EVT:
      btif_avk_rc_handler(event, (tBTA_AVK*)p_data);
      break;


    default:
      BTIF_TRACE_WARNING("%s: unhandled event=%s", __func__,
                         dump_avk_sm_event_name((btif_avk_sm_event_t)event));
      return false;
  }
  return true;
}

/*****************************************************************************
 *
 * Function     btif_avk_state_opened_handler
 *
 * Description  Handles AV events while AVDTP is in OPEN state
 *
 * Returns      true if event was processed, false otherwise
 *
 ******************************************************************************/

static bool btif_avk_state_opened_handler(btif_sm_event_t event, void* p_data,
                                         int index) {
  //RawAddress * bt_addr = NULL;
  tBTA_AVK* p_av = (tBTA_AVK*)p_data;

  BTIF_TRACE_IMP("%s event:%s flags %x peer_sep %x and index %x reconfig_event: %d",
     __func__, dump_avk_sm_event_name((btif_avk_sm_event_t)event),
     btif_avk_cb[index].flags, btif_avk_cb[index].peer_sep, index, btif_avk_cb[index].reconfig_event);

  if ((event == BTA_AVK_REMOTE_CMD_EVT) &&
      (p_av->remote_cmd.rc_id == BTA_AVK_RC_PLAY)) {
    if (btif_avk_check_flag_remote_suspend(index)) {
      BTIF_TRACE_EVENT("%s: Resetting remote suspend flag on RC PLAY", __func__);
      btif_avk_clear_remote_suspend_flag();
      if (bluetooth::headset::btif_hf_is_call_vr_idle())
      {
        RawAddress addr = btif_avk_rc_get_connected_peer_address(p_av->remote_cmd.rc_handle);
        if (!addr.IsEmpty() && btif_avk_is_current_device(addr)) {
          BTIF_TRACE_EVENT("%s: No active call, start stream for active device ", __func__);
          btif_avk_dispatch_sm_event(BTIF_AVK_START_STREAM_REQ_EVT, NULL, 0);
        }
      }
    }
  }

  switch (event) {
    case BTIF_SM_ENTER_EVT: {
      btif_avk_cb[index].flags &= ~BTIF_AVK_FLAG_PENDING_STOP;
      btif_avk_cb[index].flags &= ~BTIF_AVK_FLAG_PENDING_START;

#if (TWS_ENABLED == TRUE)
      if (btif_avk_cb[index].tws_device) {
        APPL_TRACE_DEBUG("TWS+ device enter opened state");
        for(int i = 0; i < btif_max_avk_clients; i++) {
          if (i != index && btif_avk_cb[i].tws_device &&
              (btif_avk_cb[i].flags & BTIF_AVK_FLAG_PENDING_START)) {
            APPL_TRACE_DEBUG("The other TWS+ is pending start, sending bta av start");
            btif_avk_cb[index].flags |= BTIF_AVK_FLAG_PENDING_START;
            BTA_AvkStart(btif_avk_cb[index].bta_handle);
          }
        }
      }
#endif
    } break;

    case BTIF_SM_EXIT_EVT: {
      btif_avk_cb[index].flags &= ~BTIF_AVK_FLAG_PENDING_START;
    } break;

    case BTIF_AVK_START_STREAM_REQ_EVT: {
      /* update multicast state here if new device is connected
       * after A2dp connection. New A2dp device is connected
       * whlie playing */
      is_codec_config_dump = true;
      if (btif_avk_cb[index].peer_sep == AVDT_TSEP_SRC) {
        BTA_AvkStart(btif_avk_cb[index].bta_handle);
        btif_avk_cb[index].flags |= BTIF_AVK_FLAG_PENDING_START;
      }
    } break;

    case BTA_AVK_START_EVT: {
      BTIF_TRACE_DEBUG("BTA_AVK_START_EVT status %d, suspending %d, init %d",
          p_av->start.status, p_av->start.suspending, p_av->start.initiator);
      BTIF_TRACE_DEBUG("BTA_AVK_START_EVT role: %d", p_av->start.role);
      if (p_av->start.role == HOST_ROLE_SLAVE) {
        btif_avk_cb[index].is_slave = true;
      } else {
        // update if we are master after role switch before start
        btif_avk_cb[index].is_slave = false;
      }
      /* There can be role switch after device is connected,
       * hence check for role before starting multicast, and
       * disable if we are in slave role for any connection
       */

      if ((p_av->start.status == BTA_SUCCESS) && (p_av->start.suspending == true))
        return true;

      /* if remote tries to start a2dp when call is in progress, suspend it right away */
      if ((!(btif_avk_cb[index].flags & BTIF_AVK_FLAG_PENDING_START)) &&
            (!bluetooth::headset::btif_hf_is_call_vr_idle())) {
        BTIF_TRACE_EVENT("%s: trigger suspend as call is in progress!!", __func__);
        btif_avk_cb[index].flags &= ~BTIF_AVK_FLAG_PENDING_START;
        btif_avk_cb[index].remote_started = true;
        btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_STARTED);
        btif_avk_dispatch_sm_event(BTIF_AVK_REMOTE_SUSPEND_STREAM_REQ_EVT, &index, sizeof(index));
        break;
      }

      /* if remote tries to start a2dp when DUT is a2dp source
       * then suspend. In case a2dp is sink and call is active
       * then disconnect the AVDTP channel
       */
#if (TWS_ENABLED == TRUE)
      if (btif_avk_cb[index].tws_device &&
        (btif_avk_cb[index].flags & BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING)) {
        /* Susupend initiated after start to earbuds but in some scenario
         * if one of the earbuds took more time to respond to start req or
         * earbud sent suspend for dut initiated start and then sent start again
         * We have to suspend stream
         */
        BTIF_TRACE_DEBUG("%s:Suspending pending for TWS pair",__func__);
        btif_avk_dispatch_sm_event(BTIF_AVK_SUSPEND_STREAM_REQ_EVT, NULL, 0);
      }
#endif
      /* remain in open state if status failed */
      /* Multicast-soft Handoff:
       * START failed, cleanup Handoff flag.
       */
      if (p_av->start.status != BTA_AVK_SUCCESS)
      {
        int i;

          /* Clear dual handoff flag */
          for(i = 0; i < btif_max_avk_clients; i++)
              btif_avk_cb[i].dual_handoff = false;

          return false;
      }

#ifndef AVK_BACKPORT
      if (btif_avk_cb[index].peer_sep == AVDT_TSEP_SRC)
          btif_a2dp_sink_set_rx_flush(false); /*  remove flush state, ready for streaming*/
#endif
      btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_STARTED);
    } break;



    case BTIF_AVK_DISCONNECT_REQ_EVT: {
      BTA_AvkClose(btif_avk_cb[index].bta_handle);
      if (btif_avk_cb[index].peer_sep == AVDT_TSEP_SRC) {
        BTA_AvkCloseRc(btif_avk_cb[index].bta_handle);
      }

/* SPLITA2DP */
/* SPLITA2DP */
      /* change state to idle, send acknowledgement if start is pending */
      if (btif_avk_cb[index].flags & BTIF_AVK_FLAG_PENDING_START) {
        btif_a2dp_command_ack(A2DP_CTRL_ACK_FAILURE);
        btif_avk_cb[index].flags &= ~BTIF_AVK_FLAG_PENDING_START;
      }
      btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_CLOSING);
      /* inform the application that we are disconnecting */
      btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTING,
                                   &(btif_avk_cb[index].peer_bda));
    } break;

    case BTA_AVK_CLOSE_EVT: {
      /* avdtp link is closed
       * Don't close the A2dp when Dual playback is happening
       */
      if (btif_avk_is_connected_on_other_idx(index)) {
        APPL_TRACE_WARNING("Conn is closing,close AV data channel");
        if ((!btif_avk_is_split_a2dp_enabled() ||
             (btif_avk_is_split_a2dp_enabled() && !reconfig_a2dp)) &&
            ((btif_avk_cb[index].flags & BTIF_AVK_FLAG_REMOTE_SUSPEND) != 0) &&
            ((btif_avk_cb[index].flags & BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING) == 0)) {
          /* fake handoff state to switch streaming to other codec device */
          btif_avk_cb[index].dual_handoff = true;
        } else if (!btif_avk_is_playing()) { //TODO yanhuig, how remove?
          APPL_TRACE_WARNING("Suspend the AV Data channel");
          /* ensure tx frames are immediately suspended */
        }
      } else {
        APPL_TRACE_WARNING("Stop the AV Data channel");
        btif_a2dp_sink_on_stopped(NULL);
      }

      /* inform the application that we are disconnected */
      btif_avk_cb[index].flags |= BTIF_AVK_FLAG_PENDING_DISCONNECT;
      btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
                                        &(btif_avk_cb[index].peer_bda));
      btif_avk_cb[index].flags &= ~BTIF_AVK_FLAG_PENDING_DISCONNECT;

      /* change state to idle, send acknowledgement if start is pending */
      if (btif_avk_cb[index].flags & BTIF_AVK_FLAG_PENDING_START) {
        btif_a2dp_command_ack(A2DP_CTRL_ACK_FAILURE);
        btif_avk_cb[index].flags &= ~BTIF_AVK_FLAG_PENDING_START;
      }

      btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_IDLE);
      btif_report_connection_state_to_ba(BTAV_CONNECTION_STATE_DISCONNECTED);
    } break;

    case BTA_AVK_RECONFIG_EVT: {
      if ((btif_avk_cb[index].flags & BTIF_AVK_FLAG_PENDING_START) &&
          (p_av->reconfig.status == BTA_AVK_SUCCESS)) {
        APPL_TRACE_WARNING("reconfig done BTA_AVstart()");
        BTA_AvkStart(btif_avk_cb[index].bta_handle);
        ba_send_message(BTIF_BA_BT_A2DP_STARTING_EVT, 0, NULL, true);
      } else if (btif_avk_cb[index].flags & BTIF_AVK_FLAG_PENDING_START) {
        btif_avk_cb[index].flags &= ~BTIF_AVK_FLAG_PENDING_START;
        btif_a2dp_command_ack(A2DP_CTRL_ACK_FAILURE);
      }
      btif_avk_cb[index].reconfig_pending = false;
    } break;

    case BTIF_AVK_CONNECT_REQ_EVT: {
      if (memcmp(((btif_avk_connect_req_t*)p_data)->target_bda, &(btif_avk_cb[index].peer_bda),
                 sizeof(btif_avk_cb[index].peer_bda)) == 0) {
        BTIF_TRACE_DEBUG("%s: Ignore BTIF_AVK_CONNECT_REQ_EVT for same device",
                         __func__);
      } else {
          btif_avk_connect_req_t* connect_req_p = (btif_avk_connect_req_t*)p_data;
          RawAddress& target_bda = *connect_req_p->target_bda;
        BTIF_TRACE_WARNING(
            "%s: Moved to opened by Other incoming Connect request: "
            "target_bda=%s",
            __func__, target_bda.ToString().c_str());
        btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
                                     &target_bda);
      }
      btif_avk_connect_req_t* connect_req_t = (btif_avk_connect_req_t*)p_data;
      if (bt_avk_sink_callbacks != NULL)
          connect_req_t->uuid = UUID_SERVCLASS_AUDIO_SINK;
      btif_queue_advance_by_uuid(connect_req_t->uuid, &(btif_avk_cb[index].peer_bda));
      btif_report_connection_state_to_ba(BTAV_CONNECTION_STATE_DISCONNECTED);
    } break;


    case BTA_AVK_RC_OPEN_EVT: {
      btif_avk_check_rc_connection_priority(p_data);
    }  break;


      CHECK_RC_EVENT(event, (tBTA_AVK*)p_data);

    default: {
      BTIF_TRACE_WARNING("%s : unhandled event:%s", __func__,
                          dump_avk_sm_event_name((btif_avk_sm_event_t)event));
      return false;
    }
  }
  return true;
}

/*****************************************************************************
 *
 * Function     btif_avk_state_started_handler
 *
 * Description  Handles AV events while A2DP stream is started
 *
 * Returns      true if event was processed, false otherwise
 *
 ******************************************************************************/

static bool btif_avk_state_started_handler(btif_sm_event_t event, void* p_data,
                                          int index) {
  tBTA_AVK* p_av = (tBTA_AVK*)p_data;
  //RawAddress *bt_addr = nullptr;

  btif_sm_state_t state = BTIF_AVK_STATE_IDLE;
  int i;
  bool hal_suspend_pending = false;
  tA2DP_CTRL_CMD pending_cmd = btif_a2dp_get_pending_command();
  bool remote_start_cancelled = false;
  BTIF_TRACE_IMP("%s event:%s flags %x  index =%d, reconfig_event: %d", __func__,
                   dump_avk_sm_event_name((btif_avk_sm_event_t)event),
                   btif_avk_cb[index].flags, index, btif_avk_cb[index].reconfig_event);

  switch (event) {
    case BTIF_SM_ENTER_EVT:
      /* Ack from entry point of started handler instead of open state to avoid
       * race condition
       */

      /* Already changed state to started, send acknowledgement
       * if start is pending
       */
      if (btif_avk_cb[index].flags & BTIF_AVK_FLAG_PENDING_START) {
          btif_avk_cb[index].flags &= ~BTIF_AVK_FLAG_PENDING_START;
      }

      /* we are again in started state, clear any remote suspend flags */
      btif_avk_cb[index].flags &= ~BTIF_AVK_FLAG_REMOTE_SUSPEND;

      if ((!enable_multicast)&&((btif_avk_cb[index].remote_started)
          && (btif_avk_is_playing_on_other_idx(index)))) {
        BTIF_TRACE_IMP("%s Do not update audio state change to app for index =%d", __func__, index);
      } else {
        /**
            * Report to components above that we have entered the streaming
            * stage, this should usually be followed by focus grant.
            * see update_audio_focus_state()
            */
          /** if DUT is in idle then only update the AVDTP_START to App
            * whether it is touchtone START are actual START.
            * else don't update to app any kind of touchtone, dialpadtones etc
            * which will not trigger actual playback.
            */
          bool is_playing = false;
          for (int i = 0; i < btif_max_avk_clients; i++) {
            if (btif_avk_cb[i].current_playing) {
              is_playing = true;
              BTIF_TRACE_DEBUG("%s: current playing index: %d, is_playing: %d",
                           __func__, btif_avk_cb[i].current_playing, is_playing);
              break;
            }
          }
          BTIF_TRACE_DEBUG("%s: is_playing: %d", __func__, is_playing);
          if (!is_playing) {
            BTIF_TRACE_DEBUG("%s: start streaming when both are in opened state", __func__);
          if (btif_avk_is_a2dp_sink_handoff_required(index)) {
            btif_avk_initiate_sink_handoff(index, true);
          }
            btif_report_audio_state(BTAV_AUDIO_STATE_STARTED, &(btif_avk_cb[index].peer_bda));
          }
      }
      btif_avk_cb[index].is_device_playing = true;
      btif_report_audio_state_to_ba(BTAV_AUDIO_STATE_STARTED);

#ifdef AVK_BACKPORT
      if (btif_avk_cb[index].peer_sep == AVDT_TSEP_SRC)
        btif_av_request_audio_focus(true);
#endif
      // Clear Dual Handoff for all SCBs for DUT initiated Start.
      //If split a2dp is enabled, clear dual handoff flag in offload resp evt.
      if ((!btif_avk_is_split_a2dp_enabled()) &&
                    (!(btif_avk_cb[index].remote_started))) {
        for(i = 0; i < btif_max_avk_clients; i++) {
          btif_avk_cb[i].dual_handoff = false;
        }
        BTIF_TRACE_IMP("%s Setting device as current playing for index = %d",
                                        __func__, index);
      } else {
          BTIF_TRACE_IMP("%s Remote Start, Not updating current playing for index = %d",
                                          __func__, index);
      }
#if (TWS_ENABLED == TRUE)
     if (btif_avk_cb[index].tws_device) {
       btif_avk_cb[index].current_playing = true;
     }
#endif
     break;

    case BTIF_SM_EXIT_EVT:
      break;

    case BTIF_AVK_START_STREAM_REQ_EVT:
      /* we were remotely started, just ack back the local request */
      if (btif_avk_cb[index].remote_started) btif_avk_cb[index].remote_started = false;
      break;



    /* fixme -- use suspend = true always to work around issue with BTA AV */
    case BTIF_AVK_STOP_STREAM_REQ_EVT:
    case BTIF_AVK_SUSPEND_STREAM_REQ_EVT:
      /* set pending flag to ensure btif task is not trying to restart
       * stream while suspend is in progress.
       * Multicast: If streaming is happening on both devices, we need
       * to update flag for both connections as SUSPEND request will
       * be sent to only one stream as internally BTA takes care of
       * suspending both streams.
       */
      if (enable_multicast
#if (TWS_ENABLED == TRUE)
          || (btif_avk_cb[index].tws_device
          && ((btif_avk_cb[index].flags & BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING) == 0))
#endif
         ) {
        //TODO check for tws pair
        for(i = 0; i < btif_max_avk_clients; i++) {
          state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
          if (state == BTIF_AVK_STATE_STARTED
#if (TWS_ENABLED == TRUE)
            //Will reach here if TWS+ pair is not in started state
            || (btif_avk_cb[index].tws_device &&
               i != index && btif_avk_cb[i].tws_device &&
               (btif_avk_cb[i].flags & BTIF_AVK_FLAG_PENDING_START))
#endif
             )
            btif_avk_cb[i].flags |= BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING;
        }
      } else {
        btif_avk_cb[index].flags |= BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING;
      }
      /* if we were remotely suspended but suspend locally, local suspend
       * always overrides
       */
      btif_avk_cb[index].flags &= ~BTIF_AVK_FLAG_REMOTE_SUSPEND;

      if (btif_avk_cb[index].peer_sep == AVDT_TSEP_SRC) {
        btif_a2dp_sink_set_rx_flush(true);
        btif_a2dp_sink_on_stopped(NULL);
      }

      BTA_AvkStop(true, btif_avk_cb[index].bta_handle);
      break;

    case BTIF_AVK_DISCONNECT_REQ_EVT:
      // Now it is not the current playing
      // request avdtp to close
      BTA_AvkClose(btif_avk_cb[index].bta_handle);
      if (btif_avk_cb[index].peer_sep == AVDT_TSEP_SRC)
          BTA_AvkCloseRc(btif_avk_cb[index].bta_handle);

      if (btif_avk_is_split_a2dp_enabled() &&
        btif_avk_is_connected_on_other_idx(index)) {
        /*Fake handoff state to switch streaming to other coddeced
          device */
        //TODO change will be removed after official patch
        //btif_avk_cb[index].dual_handoff = true;
      } else {
        if (btif_a2dp_audio_if_init) {
        }
      }
#if (TWS_ENABLED == TRUE)
      if (btif_avk_cb[index].dual_handoff &&
        btif_avk_cb[index].tws_device &&
        btif_avk_is_tws_device_playing(index)) {
        BTIF_TRACE_DEBUG("%s:Playing on TWSP pair, do not reconfig",__func__);
        btif_avk_cb[index].dual_handoff = false;
      }
#endif
      // inform the application that we are disconnecting
      btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTING, &(btif_avk_cb[index].peer_bda));

      // wait in closing state until fully closed
      btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_CLOSING);
      break;

    case BTA_AVK_SUSPEND_EVT:
      BTIF_TRACE_EVENT("BTA_AVK_SUSPEND_EVT status %d, init %d, flag %d",
           p_av->suspend.status, p_av->suspend.initiator, btif_avk_cb[index].flags);
      // Check if this suspend is due to DUAL_Handoff
      if ((btif_avk_cb[index].dual_handoff) &&
          (p_av->suspend.status == BTA_AVK_SUCCESS)) {
        if (!btif_avk_is_split_a2dp_enabled()){
          uint8_t curr_hdl = btif_avk_cb[index].bta_handle;
          uint8_t* cur_codec_cfg = NULL;
          uint8_t* old_codec_cfg = NULL;
          cur_codec_cfg = bta_avk_co_get_peer_codec_info(curr_hdl);
          if (cur_codec_cfg != NULL) {
           BTIF_TRACE_EVENT("BTA_AVK_SUSPEND_EVT: Current codec = ");
           for (int i = 0; i < AVDT_CODEC_SIZE; i++)
             BTIF_TRACE_EVENT("%d ",cur_codec_cfg[i]);
          }
          int other_index = btif_avk_get_other_connected_idx(index);
          if (other_index != INVALID_INDEX) {
            uint8_t other_hdl = btif_avk_cb[other_index].bta_handle;
            old_codec_cfg = bta_avk_co_get_peer_codec_info(other_hdl);
          }
          if (old_codec_cfg != NULL) {
           BTIF_TRACE_EVENT("BTA_AVK_SUSPEND_EVT: Old codec = ");
           for (int i = 0; i < AVDT_CODEC_SIZE; i++)
             BTIF_TRACE_EVENT("%d ",old_codec_cfg[i]);
          }
          /*In P implementation Audio is sending suspend for same codec SHO and different codec SHO*/

          btif_avk_dispatch_sm_event(BTIF_AVK_SETUP_CODEC_REQ_EVT, NULL, 0);
          is_block_hal_start = true;
          btif_trigger_unblock_audio_start_recovery_timer();
          BTIF_TRACE_EVENT("BTA_AVK_SUSPEND_EVT: Wait for Audio Start in non-split");

        } else {
          BTIF_TRACE_EVENT("BTA_AVK_SUSPEND_EVT:SplitA2DP Disallow stack start wait Audio to Start");
          audio_start_awaited = true;
        }
      } else {
        for (int i = 0; i < btif_max_avk_clients; i++) {
          if (i != index && (btif_avk_cb[i].dual_handoff == true))
            btif_avk_cb[i].dual_handoff = false;
        }
      }
      if (btif_avk_cb[index].remote_started) {
        btif_avk_cb[index].remote_started = false;
      }
      else if (p_av->suspend.initiator != true) {
        /* remote suspend, notify HAL and await audioflinger to
         * suspend/stop stream
         * set remote suspend flag to block media task from restarting
         * stream only if we did not already initiate a local suspend
         * set remote suspend flag before suspending stream as in race conditions
         * when stream is suspended, but flag is things ge tossed up
         */
        BTIF_TRACE_EVENT("Clear before suspending");
        if ((btif_avk_cb[index].flags & BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING) == 0) {
          btif_avk_cb[index].flags |= BTIF_AVK_FLAG_REMOTE_SUSPEND;
          bta_avk_sniff_enable(false, btif_avk_cb[index].peer_bda);
        }
      }

      /* a2dp suspended, stop media task until resumed
       * Multicast: If streaming on other device, don't call onsuspended
       * as it unblocks the audio process and audio process may send
       * subsequent commands and create problem during the time where we
       * still did not receive response for SUSPEND sent to other device.
       * Keep the suspend failure handling untouched and handle
       * only success case to check and avoid calling onsuspended.
       */
      if (pending_cmd == A2DP_CTRL_CMD_SUSPEND ||
          pending_cmd == A2DP_CTRL_CMD_STOP) {
        BTIF_TRACE_DEBUG("HAL suspend/stop pending ack the suspend");
        hal_suspend_pending = true;
      }
      if ((p_av->suspend.status != BTA_AVK_SUCCESS) ||
          hal_suspend_pending ||
          (!btif_avk_is_playing_on_other_idx(index)) ||
          (btif_avk_is_playing_on_other_idx(index) &&
          (btif_avk_get_is_remote_started_idx() ==
          (btif_max_avk_clients - (index +1))))) {
          BTIF_TRACE_DEBUG("Other device suspended/Remote started, ack the suspend");
          btif_avk_a2dp_on_suspended(&p_av->suspend);
      } else if(btif_avk_is_playing_on_other_idx(index)) {
        BTIF_TRACE_DEBUG("Other device not suspended, don't ack the suspend");
      }

      BTIF_TRACE_DEBUG("%s: local suspend flag: %d, reconfig_event: %d", __func__,
              btif_avk_cb[index].flags & BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING,
              btif_avk_cb[index].reconfig_event);

      if ((!enable_multicast)&& btif_avk_cb[index].is_suspend_for_remote_start
            && (btif_avk_is_playing_on_other_idx(index)))
      {
        BTIF_TRACE_IMP("%s Don't update audio state change to app for idx =%d", __func__, index);
        btif_avk_cb[index].is_device_playing = false;
      } else {
        if (!((btif_avk_cb[index].flags & BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING)
                                          || (p_av->suspend.initiator == true)))
        {
          btif_report_audio_state(BTAV_AUDIO_STATE_REMOTE_SUSPEND, &(btif_avk_cb[index].peer_bda));
        }
        else
        {
          btif_report_audio_state(BTAV_AUDIO_STATE_STOPPED, &(btif_avk_cb[index].peer_bda));
        }
      }

      remote_start_cancelled = false;
      // if not successful, remain in current state
      if (p_av->suspend.status != BTA_AVK_SUCCESS) {
        if (btif_avk_cb[index].is_suspend_for_remote_start) {
          BTIF_TRACE_DEBUG("Suspend sent for remote start failed");
          btif_avk_cb[index].is_suspend_for_remote_start = false;
          if (!btif_avk_is_playing_on_other_idx(index) &&
                  (index == btif_avk_get_latest_device_idx_to_start())) {
            BTIF_TRACE_DEBUG("other index not playing, setup codec");
            btif_avk_dispatch_sm_event(BTIF_AVK_SETUP_CODEC_REQ_EVT, NULL, 0);
          }
        }

        btif_avk_cb[index].flags &= ~BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING;

        return false;
      }
      btif_avk_cb[index].is_suspend_for_remote_start = false;
      btif_avk_cb[index].is_device_playing = false;
      btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_OPENED);
      btif_report_audio_state_to_ba(BTAV_AUDIO_STATE_REMOTE_SUSPEND);

      // suspend completed and state changed, clear pending status
      btif_avk_cb[index].flags &= ~BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING;


      /* clear delay record array when stream suspended */
      BTIF_TRACE_DEBUG("clear delay record array when stream suspended");
      delay_record_idx = 0;
      average_delay = 0;
      memset(delay_record, 0, sizeof(int64_t) * DELAY_RECORD_COUNT);
      break;

    case BTA_AVK_STOP_EVT:
      btif_avk_cb[index].flags |= BTIF_AVK_FLAG_PENDING_STOP;
      BTIF_TRACE_DEBUG("Stop the AV Data channel");
      btif_a2dp_sink_on_stopped(NULL);
      btif_avk_cb[index].is_device_playing = false;

      if ((!enable_multicast)&& btif_avk_cb[index].is_suspend_for_remote_start
          && (btif_avk_is_playing_on_other_idx(index))) {
        BTIF_TRACE_IMP("%s Do not update audio state change to app for index =%d", __FUNCTION__, index);
        btif_avk_cb[index].is_device_playing = FALSE;
      } else {
        btif_report_audio_state(BTAV_AUDIO_STATE_STOPPED, &(btif_avk_cb[index].peer_bda));
      }
      btif_avk_cb[index].is_suspend_for_remote_start = FALSE;
      // if stop was successful, change state to open
      if (p_av->suspend.status == BTA_AVK_SUCCESS)
        btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_OPENED);

      btif_report_audio_state_to_ba(BTAV_AUDIO_STATE_STOPPED);
      break;

    case BTA_AVK_CLOSE_EVT:
      btif_avk_cb[index].flags |= BTIF_AVK_FLAG_PENDING_STOP;

/* SPLITA2DP */
      if (!btif_avk_is_connected_on_other_idx(index)) {
        if (btif_avk_is_split_a2dp_enabled()) {
        }
      }
/* SPLITA2DP */
      /* avdtp link is closed */
      btif_a2dp_sink_on_stopped(NULL);

      /* inform the application that we are disconnected */
      btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
                                   &(btif_avk_cb[index].peer_bda));

      btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_IDLE);
      btif_report_connection_state_to_ba(BTAV_CONNECTION_STATE_DISCONNECTED);
      break;

    case BTA_AVK_RC_OPEN_EVT:
      btif_avk_check_rc_connection_priority(p_data);
      break;

    case BTIF_AVK_SETUP_CODEC_REQ_EVT: {
      uint8_t hdl = btif_avk_get_av_hdl_from_idx(index);
      APPL_TRACE_DEBUG("%s: hdl = %d",__func__, hdl);
      if (hdl >= 0)
        btif_a2dp_source_setup_codec(hdl);
      }
      break;

    case BTA_AVK_DELAY_REPORT_EVT:
      /* Todo Implement: Delay report during A2dp streaming
       * Update latency to A2dp StateMachine if need support
       * dynamic audio/video sync */
      break;

    case BTIF_AVK_SINK_QUICK_HANDOFF_EVT: {
        RawAddress* addr = (RawAddress *)p_data;
        btif_report_audio_state(BTAV_AUDIO_STATE_STARTED, addr);
      }
      break;

      CHECK_RC_EVENT(event, (tBTA_AVK*)p_data);

    default:
      BTIF_TRACE_WARNING("%s: unhandled event=%s", __func__,
                         dump_avk_sm_event_name((btif_avk_sm_event_t)event));
      return false;
  }

  return true;
}

/*****************************************************************************
 *  Local event handlers
 *****************************************************************************/

static void btif_avk_handle_event(uint16_t event, char* p_param) {
  int index = 0;
  tBTA_AVK *p_bta_data = (tBTA_AVK*)p_param;
  RawAddress *bt_addr = nullptr;
  uint8_t role;
  //int uuid;
  //bool active_device_selected = false;
  //int previous_active_index = INVALID_INDEX;
  //int now_active_index = INVALID_INDEX;

  switch (event) {
    case BTIF_AVK_INIT_REQ_EVT:
      BTIF_TRACE_DEBUG("%s: BTIF_AVK_INIT_REQ_EVT", __func__);
      btif_a2dp_sink_on_init();
      break;

    case BTIF_AVK_CLEANUP_REQ_EVT: // Clean up to be called on default index
      BTIF_TRACE_DEBUG("%s: BTIF_AVK_CLEANUP_REQ_EVT", __func__);
      if (bt_avk_sink_callbacks) {
          bt_avk_sink_callbacks = NULL;
        }
      btif_a2dp_sink_shutdown();
      //if (bt_av_src_callbacks != NULL) //TODO: how to avoid deinit audio interface when src exsiting
       // break;
      if (btif_a2dp_audio_if_init) {
        btif_a2dp_audio_if_init = false;
        btif_a2dp_audio_interface_deinit();
      }
      return;

    case BTIF_AVK_CONNECT_REQ_EVT:
      if (p_param != NULL) {
        btif_avk_connect_req_t* connect_req_p = (btif_avk_connect_req_t*)p_param;
        bt_addr = connect_req_p->target_bda;
        index = btif_avk_idx_by_bdaddr(bt_addr);
        if (index == btif_max_avk_clients) {
          index = 0;
        }
        BTIF_TRACE_DEBUG("%s: BTIF_AVK_CONNECT_REQ_EVT on idx = %d", __func__, index);
      }
      break;

    case BTIF_AVK_DISCONNECT_REQ_EVT:
      // Bd address passed should help us in getting the handle
      bt_addr = (RawAddress *)p_param;
      index = btif_avk_idx_by_bdaddr(bt_addr);
      break;

    case BTIF_AVK_SINK_CONFIG_REQ_EVT: {
      btif_avk_sink_config_req_t *config = (btif_avk_sink_config_req_t *)p_param;
      index = btif_avk_idx_by_bdaddr(&config->peer_bd);
      BTIF_TRACE_DEBUG("%s: device %s index = %d", __func__,
          config->peer_bd.ToString().c_str(), index);
      } break;

    case BTIF_AVK_START_STREAM_REQ_EVT:
      /* Get the last connected device on which START can be issued
       * Get the Dual A2dp Handoff Device first, if none is present,
       * go for lastest connected.
       * In A2dp Multicast, the index selected can be any of the
       * connected device. Stack will ensure to START the steaming
       * on both the devices. */
      index = btif_avk_get_latest_device_idx_to_start();
#if (TWS_ENABLED == TRUE)
      if (btif_avk_current_device_is_tws()) {
        int started_index = btif_avk_get_latest_playing_device_idx();
        if (btif_avk_stream_started_ready() &&
          started_index == index) {
          index = btif_avk_get_tws_pair_idx(started_index);
          BTIF_TRACE_DEBUG("Switching start req from index %d to %d",started_index,index);
        }
      }
#endif
      break;
    case BTIF_AVK_STOP_STREAM_REQ_EVT:
    case BTIF_AVK_SUSPEND_STREAM_REQ_EVT:
      // Should be handled by current STARTED
      if(p_param == NULL)
        index = btif_avk_get_latest_stream_device_idx();
      else
        index = (int)*p_param;
      BTIF_TRACE_DEBUG("BTIF_AVK_SUSPEND_STREAM_REQ_EVT:revising effective index %d", index);
      break;
    case BTIF_AVK_REINIT_AUDIO_IF:
      btif_avk_reinit_audio_interface();
      return;
    case BTIF_AVK_REMOTE_SUSPEND_STREAM_REQ_EVT:
      index = (int) *p_param;
      if (index != INVALID_INDEX && index > btif_max_avk_clients &&
        btif_avk_cb[index].remote_started == false)
        index = btif_max_avk_clients;
      if (index >= btif_max_avk_clients) {
        BTIF_TRACE_ERROR("%s: Invalid index for connection", __func__);
        return;
      }
      BTIF_TRACE_IMP("%s: Remote Started set @ index = %d", __func__, index);
      btif_avk_clear_remote_start_timer(index);
      event = BTIF_AVK_SUSPEND_STREAM_REQ_EVT;
      break;
    case BTIF_AVK_RESET_REMOTE_STARTED_FLAG_UPDATE_AUDIO_STATE_EVT:
      index = (int) *p_param;
      if (index >= 0 && index < btif_max_avk_clients) {
        if (btif_avk_cb[index].peer_sep == AVDT_TSEP_SNK)
        {
          BTIF_TRACE_IMP("%s: on remote start clean up update audio started state for index %d",
                          __func__, index);
          btif_report_audio_state(BTAV_AUDIO_STATE_STARTED, &(btif_avk_cb[index].peer_bda));
        }
        btif_avk_cb[index].is_device_playing = true;
      }
    case BTIF_AVK_RESET_REMOTE_STARTED_FLAG_EVT:
      btif_avk_reset_remote_started_flag();
      return;
      // Events from the stack, BTA
    case BTA_AVK_ENABLE_EVT:
      index = 0;
      break;

    case BTA_AVK_REGISTER_EVT:
      index = HANDLE_TO_INDEX(p_bta_data->registr.hndl);
      break;

    case BTA_AVK_OPEN_EVT:
      index = HANDLE_TO_INDEX(p_bta_data->open.hndl);
      break;

    case BTA_AVK_ROLE_CHANGED_EVT:
      index = HANDLE_TO_INDEX(p_bta_data->role_changed.hndl);
      role = p_bta_data->role_changed.new_role;
      BTIF_TRACE_EVENT("Role change: 0x%x: new role: %s",
      p_bta_data->role_changed.hndl, (role == HOST_ROLE_SLAVE) ? "Slave" : "Master");
      if (index >= 0 && index < btif_max_avk_clients) {
        btif_avk_cb[index].is_slave = (role == HOST_ROLE_SLAVE) ? true : false;
      } else {
        BTIF_TRACE_ERROR("%s: Invalid index for connection", __func__);
      }
      return;

    case BTA_AVK_PENDING_EVT:
        /* In race conditions, outgoing and incoming connections
        * at same time check for BD address at index and if it
        * does not match then check for first avialable index.
        */
        index = HANDLE_TO_INDEX(p_bta_data->pend.hndl);
        if (index >= 0 && index < btif_max_avk_clients &&
            (p_bta_data->pend.bd_addr == btif_avk_cb[index].peer_bda))
       {
            BTIF_TRACE_EVENT("incomming connection at index %d", index);
       }
       else
       {
           RawAddress empty_addr = RawAddress::kEmpty;
           index = btif_avk_idx_by_bdaddr(&empty_addr);
           if (index >= btif_max_avk_clients)
           {
               BTIF_TRACE_ERROR("No free SCB available");
               BTA_AvkDisconnect(p_bta_data->pend.bd_addr);
           }
           else
           {
               BTIF_TRACE_EVENT("updated index for connection %d", index);
           }
       }
       break;
    case BTA_AVK_REJECT_EVT:
      index = HANDLE_TO_INDEX(p_bta_data->reject.hndl);
      break;

    case BTA_AVK_STOP_EVT:
      index = HANDLE_TO_INDEX(p_bta_data->suspend.hndl);
      break;

    case BTA_AVK_CLOSE_EVT:
      index = HANDLE_TO_INDEX(p_bta_data->close.hndl);
      break;

    case BTA_AVK_START_EVT:
      index = HANDLE_TO_INDEX(p_bta_data->start.hndl);
      break;

    case BTA_AVK_RECONFIG_EVT:
      index = HANDLE_TO_INDEX(p_bta_data->reconfig.hndl);
      break;

    case BTA_AVK_SUSPEND_EVT:
      index = HANDLE_TO_INDEX(p_bta_data->suspend.hndl);
      break;

    case BTA_AVK_DELAY_REPORT_EVT:
      index = HANDLE_TO_INDEX(p_bta_data->delay_rpt.hndl);
      break;

      /* Handle all RC events on default index. RC handling should take
       * care of the events. All events come with BD Address
       * Handled well in AV Opening, opened and started state
       * AV Idle handler needs to take care of this event properly.
       */
    case BTA_AVK_RC_OPEN_EVT:
      index = btif_avk_get_valid_idx_for_rc_events(p_bta_data->rc_open.peer_addr,
                p_bta_data->rc_open.rc_handle);
      break;

    case BTA_AVK_RC_CLOSE_EVT:
      /* If there is no entry in the connection table
       * RC handler has to be called for cleanup.
       * Directly call the RC handler as we cannot
       * associate any AV handle to it.
       */
      index = btif_avk_idx_by_bdaddr(&p_bta_data->rc_close.peer_addr);
      BTIF_TRACE_DEBUG("%s: BTA_AVK_RC_CLOSE_EVT: peer_addr=%s, index=%d", __func__,
                  p_bta_data->rc_close.peer_addr.ToString().c_str(), index);
      if (index == btif_max_avk_clients || btif_avk_cb[index].sm_handle == NULL)
        btif_avk_rc_handler(event, (tBTA_AVK*)p_bta_data);
      break;

    case BTA_AVK_RC_COLL_DETECTED_EVT:
      index = btif_avk_idx_by_bdaddr(&p_bta_data->rc_col_detected.peer_addr);
      break;

      /* Let the RC handler decide on these passthrough cmds
       * Use rc_handle to get the active AV device and use that mapping.
       */
    case BTA_AVK_REMOTE_CMD_EVT:
    case BTA_AVK_VENDOR_CMD_EVT:
    case BTA_AVK_META_MSG_EVT:
    case BTA_AVK_RC_FEAT_EVT:
      index = 0;
      BTIF_TRACE_EVENT("RC events: on index = %d", index);
      break;
    case BTIF_AVK_SETUP_CODEC_REQ_EVT:
      index = btif_avk_get_latest_device_idx_to_start();
      break;
    case BTA_AVK_SINK_MEDIA_CFG_EVT:
      index = btif_avk_get_current_playing_dev_idx();
      break;
  /* FALLTHROUGH */
  default:
    BTIF_TRACE_ERROR("Unhandled event = %d", event);
    break;
  }
  BTIF_TRACE_DEBUG("Handle the AV event = %x on index = %d", event, index);
  if (index >= 0 && index < btif_max_avk_clients)
      btif_sm_dispatch(btif_avk_cb[index].sm_handle, event, (void*)p_param);
  else
      BTIF_TRACE_ERROR("Unhandled Index = %d", index);
  btif_avk_event_free_data(event, p_param);

}

void btif_avk_reset_remote_started_flag()
{
    int i;
    BTIF_TRACE_DEBUG("btif_avk_reset_remote_started_flag");
    for (i = 0; i < btif_max_avk_clients; i++)
    {
        if (btif_avk_cb[i].remote_started)
            btif_avk_cb[i].remote_started = false;
    }
}

/*******************************************************************************
 *
 * Function         btif_avk_get_valid_idx
 *
 * Description      Check the validity of the current index for the connection
 *
 * Returns          Boolean
 *
 ******************************************************************************/
static bool btif_avk_get_valid_idx(int idx) {
  btif_sm_state_t state = btif_sm_get_state(btif_avk_cb[idx].sm_handle);
  return ((state == BTIF_AVK_STATE_OPENED) ||
          (state ==  BTIF_AVK_STATE_STARTED) ||
          (state == BTIF_AVK_STATE_OPENING) ||
          (state == BTIF_AVK_STATE_CLOSING));
}

/*******************************************************************************
 *
 * Function         btif_avk_idx_by_bdaddr
 *
 * Description      Get the index corresponding to BD addr
 *
 * Returns          int
 *
 ******************************************************************************/
int btif_avk_idx_by_bdaddr(RawAddress *bd_addr) {
  int i;
  for (i = 0; i < btif_max_avk_clients; i++)
    if (*bd_addr == btif_avk_cb[i].peer_bda)
      return i;
  return i;
}

bool btif_avk_is_current_device(RawAddress address) {
  int index;

  index = btif_avk_idx_by_bdaddr(&address);
  if ((index < btif_max_avk_clients) && (btif_avk_cb[index].current_playing)
      && (!btif_avk_cb[index].remote_started))
    return true;
  return false;
}

/*******************************************************************************
 *
 * Function         btif_avk_get_latest_device_idx_to_start
 *
 * Description      Get the index of the AV where streaming is to be started
 *
 * Returns          int
 *
 ******************************************************************************/
int btif_avk_get_latest_device_idx_to_start() {
  int i;
  for (i = 0; i < btif_max_avk_clients; i++)
    if (btif_avk_cb[i].current_playing)
      break;
  if (i == btif_max_avk_clients)
    BTIF_TRACE_ERROR("%s:No valid active device found",__func__);
  return i;
}

/*******************************************************************************
 *
 * Function         btif_avk_get_latest_playing_device_idx
 *
 * Description      Get the index of AV where streaming is happening
 *
 * Returns          int
 *
 ******************************************************************************/
int btif_avk_get_latest_playing_device_idx() {
  int i;
  btif_sm_state_t state;
  for (i = 0; i < btif_max_avk_clients; i++) {
    state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
    if (state == BTIF_AVK_STATE_STARTED) {
      BTIF_TRACE_IMP("Latest playing device index %d", i);
      break;
    }
  }
  return i;
}

/*******************************************************************************
 *
 * Function         btif_avk_get_latest_stream_device_idx
 *
 * Description      Get the index of AV where streaming is happening but not
                    remote started index
 *
 * Returns          int
 *
 ******************************************************************************/
int btif_avk_get_latest_stream_device_idx() {
  int i;
  btif_sm_state_t state;
  for (i = 0; i < btif_max_avk_clients; i++) {
    state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
    if ((state == BTIF_AVK_STATE_STARTED) && !(btif_avk_cb[i].remote_started)) {
      BTIF_TRACE_ERROR("Latest playing device index is %d ", i);
      break;
    }
  }
  return i;
}


/*******************************************************************************
 *
 * Function         btif_avk_get_is_remote_started_idx
 *
 * Description      Get the index of AV where last remote Start is received
 *
 * Returns          int
 *
 ******************************************************************************/
int btif_avk_get_is_remote_started_idx() {
  int index = btif_a2dp_source_last_remote_start_index();
  if (!btif_avk_cb[index].remote_started)
    index = btif_max_avk_clients;
  return index;
}

/*******************************************************************************
 *
 * Function         btif_avk_is_playing
 *
 * Description      Is AV in streaming state
 *
 * Returns          bool
 *
 ******************************************************************************/
bool btif_avk_is_playing() {
  int i;
  for (i = 0; i < btif_max_avk_clients; i++) {
    btif_avk_cb[i].state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
    if (btif_avk_cb[i].state == BTIF_AVK_STATE_STARTED) {
      BTIF_TRACE_EVENT("btif_avk_is_playing on index= %d", i);
      return true;
    }
  }
  return false;
}

/*******************************************************************************
 *
 * Function         btif_avk_get_conn_state_of_device
 *
 * Description      Returns the state of AV scb
 *
 * Returns          int
 *
 ******************************************************************************/
static int btif_avk_get_conn_state_of_device(RawAddress address) {
  btif_sm_state_t state = BTIF_AVK_STATE_IDLE;
  int i;
  for (i = 0; i < btif_max_avk_clients; i++) {
    if ( address == btif_avk_cb[i].peer_bda ) {
      state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
      //BTIF_TRACE_EVENT("BD Found: %02X %02X %02X %02X %02X %02X :state: %s",
         // address[5], address[4], address[3],
         // address[2], address[1], address[0],
         // dump_avk_sm_state_name((btif_avk_state_t)state));
    }
  }
  return state;
}

/*******************************************************************************
 *
 * Function         btif_avk_get_valid_idx_for_rc_events
 *
 * Description      gets th valid index for the RC event address
 *
 * Returns          int
 *
 ******************************************************************************/
static int btif_avk_get_valid_idx_for_rc_events(RawAddress bd_addr, int rc_handle) {
  int index = 0;
  /* First try to find if it is first event in AV IF
   * both the handles would be in IDLE state, pick the first
   * If we get second RC event while processing the priority
   * for the first, reject the second connection.
   */

  /* Get the index from connected SCBs */
  index = btif_avk_idx_by_bdaddr(&bd_addr);
  if (index == btif_max_avk_clients) {
    /* None of the SCBS matched * Allocate free SCB, null address SCB*/
    RawAddress empty_addr = RawAddress::kEmpty;
    index = btif_avk_idx_by_bdaddr(&empty_addr);
    BTIF_TRACE_EVENT("btif_avk_get_valid_idx_for_rc_events is %d", index);
    if (index >= btif_max_avk_clients) {
      BTIF_TRACE_EVENT("disconnect only AVRCP device rc_handle %d", rc_handle);
      BTA_AvkCloseRc(rc_handle);
    }
  }
  return index;
}

/*******************************************************************************
 *
 * Function         btif_avk_check_rc_connection_priority
 *
 * Description      Handles Priority callback for RC connections
 *
 * Returns          void
 *
 ******************************************************************************/
static void btif_avk_check_rc_connection_priority(void *p_data) {
  //RawAddress peer_bda;

  /*Check if it is for same AV device*/
  if (btif_avk_is_device_connected(((tBTA_AVK*)p_data)->rc_open.peer_addr)) {
    /*AV is connected */
    BTIF_TRACE_DEBUG("AV is connected, process RC connect event");
    btif_avk_rc_handler(BTA_AVK_RC_OPEN_EVT, (tBTA_AVK*)p_data);
    return;
  }
  BTIF_TRACE_DEBUG("btif_avk_check_rc_connection_priority");
  return;
}



void btif_avk_event_deep_copy(uint16_t event, char* p_dest, char* p_src) {
  BTIF_TRACE_DEBUG("%s: event: %d", __func__, event);

  if (!p_dest || !p_src) {
    BTIF_TRACE_DEBUG("%s: p_src is null", __func__);
    return;
  }
  switch (event) {
    case BTA_AVK_META_MSG_EVT: //17
      {
        BTIF_TRACE_DEBUG("%s: BTA_AVK_META_MSG_EVT", __func__);
        tBTA_AVK_META_MSG* av_src_meta_msg = (tBTA_AVK_META_MSG*)p_src;
        tBTA_AVK_META_MSG* av_dest_meta_msg = (tBTA_AVK_META_MSG*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_meta_msg));

        // First copy the structure
        maybe_non_aligned_memcpy(av_dest_meta_msg, av_src_meta_msg, sizeof(*av_src_meta_msg));
        if (av_src_meta_msg->p_data && av_src_meta_msg->len) {
            av_dest_meta_msg->p_data = (uint8_t*)osi_calloc(av_src_meta_msg->len);
          memcpy(av_dest_meta_msg->p_data, av_src_meta_msg->p_data,
                 av_src_meta_msg->len);
        }

        if (av_src_meta_msg->p_msg) {
          av_dest_meta_msg->p_msg = (tAVRC_MSG*)osi_calloc(sizeof(tAVRC_MSG));
          memcpy(av_dest_meta_msg->p_msg, av_src_meta_msg->p_msg,
                 sizeof(tAVRC_MSG));

          tAVRC_MSG* p_msg_src = av_src_meta_msg->p_msg;
          tAVRC_MSG* p_msg_dest = av_dest_meta_msg->p_msg;

          BTIF_TRACE_DEBUG("%s: opcode: 0x%x, vendor_len: %d, p_msg_src->browse.browse_len: %d",
            __func__, p_msg_src->hdr.opcode, p_msg_src->vendor.vendor_len, p_msg_src->browse.browse_len);
          if ((p_msg_src->hdr.opcode == AVRC_OP_VENDOR) &&
              (p_msg_src->vendor.p_vendor_data && p_msg_src->vendor.vendor_len)) {
            BTIF_TRACE_DEBUG("%s: AVRC_OP_VENDOR:", __func__);
            p_msg_dest->vendor.p_vendor_data =
                (uint8_t*)osi_calloc(p_msg_src->vendor.vendor_len);
            memcpy(p_msg_dest->vendor.p_vendor_data,
                   p_msg_src->vendor.p_vendor_data, p_msg_src->vendor.vendor_len);
          }
          if ((p_msg_src->hdr.opcode == AVRC_OP_BROWSE) &&
              p_msg_src->browse.p_browse_data && p_msg_src->browse.browse_len) {
            BTIF_TRACE_DEBUG("%s: AVRC_OP_BROWSE:", __func__);
            p_msg_dest->browse.p_browse_data =
               (uint8_t*)osi_calloc(p_msg_src->browse.browse_len);
            memcpy(p_msg_dest->browse.p_browse_data,
                   p_msg_src->browse.p_browse_data, p_msg_src->browse.browse_len);
            android_errorWriteLog(0x534e4554, "109699112");
          }
        }
        break;
      }

    case BTA_AVK_PENDING_EVT: //16
      {
        tBTA_AVK_PEND* av_src_pend = (tBTA_AVK_PEND*)p_src;
        tBTA_AVK_PEND* av_dest_pend = (tBTA_AVK_PEND*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_pend));
        maybe_non_aligned_memcpy(av_dest_pend, av_src_pend, sizeof(*av_src_pend));
        break;
      }

    case BTA_AVK_ENABLE_EVT: //0
      {
        tBTA_AVK_ENABLE* av_src_enable = (tBTA_AVK_ENABLE*)p_src;
        tBTA_AVK_ENABLE* av_dest_enable = (tBTA_AVK_ENABLE*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_enable));
        maybe_non_aligned_memcpy(av_dest_enable, av_src_enable, sizeof(*av_src_enable));
        break;
      }

    case BTA_AVK_REGISTER_EVT: //1
      {
        tBTA_AVK_REGISTER* av_src_reg = (tBTA_AVK_REGISTER*)p_src;
        tBTA_AVK_REGISTER* av_dest_reg = (tBTA_AVK_REGISTER*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_reg));
        maybe_non_aligned_memcpy(av_dest_reg, av_src_reg, sizeof(*av_src_reg));
        break;
      }

    case BTA_AVK_OPEN_EVT: //2
      {
        tBTA_AVK_OPEN* av_src_open = (tBTA_AVK_OPEN*)p_src;
        tBTA_AVK_OPEN* av_dest_open = (tBTA_AVK_OPEN*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_open));
        maybe_non_aligned_memcpy(av_dest_open, av_src_open, sizeof(*av_src_open));
        break;
      }

    case BTA_AVK_CLOSE_EVT: //3
      {
        tBTA_AVK_CLOSE* av_src_close = (tBTA_AVK_CLOSE*)p_src;
        tBTA_AVK_CLOSE* av_dest_close = (tBTA_AVK_CLOSE*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_close));
        maybe_non_aligned_memcpy(av_dest_close, av_src_close, sizeof(*av_src_close));
        break;
      }

    case BTA_AVK_START_EVT : //4
      {
        tBTA_AVK_START* av_src_start = (tBTA_AVK_START*)p_src;
        tBTA_AVK_START* av_dest_start = (tBTA_AVK_START*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_start));
        maybe_non_aligned_memcpy(av_dest_start, av_src_start, sizeof(*av_src_start));
        break;
      }

    case BTA_AVK_PROTECT_REQ_EVT: //6
      {
        tBTA_AVK_PROTECT_REQ* av_src_protect = (tBTA_AVK_PROTECT_REQ*)p_src;
        tBTA_AVK_PROTECT_REQ* av_dest_protect = (tBTA_AVK_PROTECT_REQ*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_protect));
        maybe_non_aligned_memcpy(av_dest_protect, av_src_protect, sizeof(*av_src_protect));
        break;
      }

    case BTA_AVK_PROTECT_RSP_EVT: //7
      {
        tBTA_AVK_PROTECT_RSP* av_src_potect_rsp = (tBTA_AVK_PROTECT_RSP*)p_src;
        tBTA_AVK_PROTECT_RSP* av_dest_potect_rsp = (tBTA_AVK_PROTECT_RSP*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_potect_rsp));
        maybe_non_aligned_memcpy(av_dest_potect_rsp, av_src_potect_rsp, sizeof(*av_src_potect_rsp));
        break;
      }

    case BTA_AVK_RC_OPEN_EVT: //8
      {
        tBTA_AVK_RC_OPEN* av_src_avrc_open = (tBTA_AVK_RC_OPEN*)p_src;
        tBTA_AVK_RC_OPEN* av_dest_avrc_open = (tBTA_AVK_RC_OPEN*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_avrc_open));
        maybe_non_aligned_memcpy(av_dest_avrc_open, av_src_avrc_open, sizeof(*av_src_avrc_open));
        break;
      }

    case BTA_AVK_RC_CLOSE_EVT: //9
      {
        tBTA_AVK_RC_CLOSE* av_src_avrc_close = (tBTA_AVK_RC_CLOSE*)p_src;
        tBTA_AVK_RC_CLOSE* av_dest_avrc_close = (tBTA_AVK_RC_CLOSE*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_avrc_close));
        maybe_non_aligned_memcpy(av_dest_avrc_close, av_src_avrc_close, sizeof(*av_src_avrc_close));
        break;
      }

    case BTA_AVK_REMOTE_CMD_EVT: //10
      {
        tBTA_AVK_REMOTE_CMD* av_src_remote_cmd = (tBTA_AVK_REMOTE_CMD*)p_src;
        tBTA_AVK_REMOTE_CMD* av_dest_remote_cmd = (tBTA_AVK_REMOTE_CMD*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_remote_cmd));
        maybe_non_aligned_memcpy(av_dest_remote_cmd, av_src_remote_cmd, sizeof(*av_src_remote_cmd));
        break;
      }

    case BTA_AVK_REMOTE_RSP_EVT: //11
      {
        tBTA_AVK_REMOTE_RSP* av_src_remote_rsp = (tBTA_AVK_REMOTE_RSP*)p_src;
        tBTA_AVK_REMOTE_RSP* av_dest_remote_rsp = (tBTA_AVK_REMOTE_RSP*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_remote_rsp));
        maybe_non_aligned_memcpy(av_dest_remote_rsp, av_src_remote_rsp, sizeof(*av_src_remote_rsp));
        break;
      }

    case BTA_AVK_VENDOR_CMD_EVT: //12 /* fall through */
    case BTA_AVK_VENDOR_RSP_EVT: //13
      {
        tBTA_AVK_VENDOR* av_src_vendor = (tBTA_AVK_VENDOR*)p_src;
        tBTA_AVK_VENDOR* av_dest_vendor = (tBTA_AVK_VENDOR*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_vendor));
        maybe_non_aligned_memcpy(av_dest_vendor, av_src_vendor, sizeof(*av_src_vendor));
        break;
      }

    case BTA_AVK_RECONFIG_EVT: //14
      {
        tBTA_AVK_RECONFIG* av_src_reconfig = (tBTA_AVK_RECONFIG*)p_src;
        tBTA_AVK_RECONFIG* av_dest_reconfig = (tBTA_AVK_RECONFIG*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_reconfig));
        maybe_non_aligned_memcpy(av_dest_reconfig, av_src_reconfig, sizeof(*av_src_reconfig));
        break;
      }

    case BTA_AVK_STOP_EVT: //5 /* fall through */
    case BTA_AVK_SUSPEND_EVT: //15
      {
        tBTA_AVK_SUSPEND* av_src_suspend = (tBTA_AVK_SUSPEND*)p_src;
        tBTA_AVK_SUSPEND* av_dest_suspend = (tBTA_AVK_SUSPEND*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_suspend));
        maybe_non_aligned_memcpy(av_dest_suspend, av_src_suspend, sizeof(*av_src_suspend));
        break;
      }

    case BTA_AVK_REJECT_EVT: //18
      {
        tBTA_AVK_REJECT* av_src_rej = (tBTA_AVK_REJECT*)p_src;
        tBTA_AVK_REJECT* av_dest_rej = (tBTA_AVK_REJECT*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_rej));
        maybe_non_aligned_memcpy(av_dest_rej, av_src_rej, sizeof(*av_src_rej));
        break;
      }

    case BTA_AVK_RC_FEAT_EVT: //19
      {
        tBTA_AVK_RC_FEAT* av_src_rc_feat = (tBTA_AVK_RC_FEAT*)p_src;
        tBTA_AVK_RC_FEAT* av_dest_rc_feat = (tBTA_AVK_RC_FEAT*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_rc_feat));
        maybe_non_aligned_memcpy(av_dest_rc_feat, av_src_rc_feat, sizeof(*av_src_rc_feat));
        break;
      }

    case BTA_AVK_RC_BROWSE_OPEN_EVT: //23
      {
        tBTA_AVK_RC_BROWSE_OPEN* av_src_browse_open = (tBTA_AVK_RC_BROWSE_OPEN*)p_src;
        tBTA_AVK_RC_BROWSE_OPEN* av_dest_browse_open = (tBTA_AVK_RC_BROWSE_OPEN*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_browse_open));
        maybe_non_aligned_memcpy(av_dest_browse_open, av_src_browse_open, sizeof(*av_src_browse_open));
        break;
      }

    case BTA_AVK_RC_BROWSE_CLOSE_EVT: //24
      {
        tBTA_AVK_RC_BROWSE_CLOSE* av_src_browse_close = (tBTA_AVK_RC_BROWSE_CLOSE*)p_src;
        tBTA_AVK_RC_BROWSE_CLOSE* av_dest_browse_close = (tBTA_AVK_RC_BROWSE_CLOSE*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_browse_close));
        maybe_non_aligned_memcpy(av_dest_browse_close, av_src_browse_close, sizeof(*av_src_browse_close));
        break;
      }

    case BTA_AVK_ROLE_CHANGED_EVT: //25
      {
        tBTA_AVK_ROLE_CHANGED* av_src_role_changed = (tBTA_AVK_ROLE_CHANGED*)p_src;
        tBTA_AVK_ROLE_CHANGED* av_dest_role_changed = (tBTA_AVK_ROLE_CHANGED*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_role_changed));
        maybe_non_aligned_memcpy(av_dest_role_changed, av_src_role_changed,
                                                           sizeof(*av_src_role_changed));
        break;
      }

    case BTA_AVK_DELAY_REPORT_EVT: //27
      {
        tBTA_AVK_DELAY_RPT* av_src_delay_rpt = (tBTA_AVK_DELAY_RPT*)p_src;
        tBTA_AVK_DELAY_RPT* av_dest_delay_rpt = (tBTA_AVK_DELAY_RPT*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_delay_rpt));
        maybe_non_aligned_memcpy(av_dest_delay_rpt, av_src_delay_rpt, sizeof(*av_src_delay_rpt));
        break;
      }

    default:
      {
        tBTA_AVK* av_src_default = (tBTA_AVK*)p_src;
        tBTA_AVK* av_dest_default = (tBTA_AVK*)p_dest;
        BTIF_TRACE_DEBUG("%s: event: %d, size: %d", __func__, event, sizeof(*av_src_default));

        // First copy the structure
        maybe_non_aligned_memcpy(av_dest_default, av_src_default, sizeof(*av_src_default));
        break;
      }
  }
}

static void btif_avk_event_free_data(btif_sm_event_t event, void* p_data) {
  BTIF_TRACE_DEBUG("%s: event: %d", __func__, event);
  switch (event) {
    case BTA_AVK_META_MSG_EVT: {
      tBTA_AVK* av = (tBTA_AVK*)p_data;
      BTIF_TRACE_DEBUG("%s: len: %d, opcode: 0x%x",
                 __func__, av->meta_msg.len, av->meta_msg.p_msg->hdr.opcode);
      if (av->meta_msg.p_data && av->meta_msg.len) {
        osi_free_and_reset((void**)&av->meta_msg.p_data);
      }

      if (av->meta_msg.p_msg) {
        BTIF_TRACE_DEBUG("%s: meta_msg.p_msg is not null: vendor_len: %d, browse_len: %d ",
          __func__, av->meta_msg.p_msg->vendor.vendor_len, av->meta_msg.p_msg->browse.browse_len);
        if (av->meta_msg.p_msg->hdr.opcode == AVRC_OP_VENDOR &&
            av->meta_msg.p_msg->vendor.vendor_len) {
          osi_free(av->meta_msg.p_msg->vendor.p_vendor_data);
        }
        if (av->meta_msg.p_msg->hdr.opcode == AVRC_OP_BROWSE &&
            av->meta_msg.p_msg->browse.browse_len) {
          osi_free(av->meta_msg.p_msg->browse.p_browse_data);
        }
        osi_free_and_reset((void**)&av->meta_msg.p_msg);
      }
    } break;

    default:
      break;
  }
}

static void bte_avk_callback(tBTA_AVK_EVT event, tBTA_AVK* p_data) {
  BTIF_TRACE_DEBUG("%s: event: %d", __func__, event);
  btif_transfer_context(btif_avk_handle_event, event, (char*)p_data,
                        sizeof(tBTA_AVK), btif_avk_event_deep_copy);
}

static void bte_avk_sink_media_callback(tBTA_AVK_EVT event,
                                       tBTA_AVK_SINK_MEDIA* p_data, RawAddress bd_addr) {
  int index = btif_avk_idx_by_bdaddr(&bd_addr);
  int cur_playing_index = btif_avk_get_current_playing_dev_idx();
  BTIF_TRACE_DEBUG("%s: index = %d, cur_playing_index: %d", 
                                           __func__, index, cur_playing_index);

  switch (event) {
    case BTA_AVK_SINK_MEDIA_DATA_EVT: {
      btif_sm_state_t state = btif_sm_get_state(btif_avk_cb[index].sm_handle);
      if (((state == BTIF_AVK_STATE_STARTED) || (state == BTIF_AVK_STATE_OPENED))
            && (index == cur_playing_index)) {
        uint8_t queue_len = btif_a2dp_sink_enqueue_buf((BT_HDR*)p_data);
        BTIF_TRACE_DEBUG("%s: index = %d, packets in sink queue %d", __func__, index, queue_len);
      }
      break;
    }
    case BTA_AVK_SINK_MEDIA_CFG_EVT: {
      btif_avk_sink_config_req_t config_req;

      /* send a command to BT Media Task */
      btif_sm_state_t av_state = btif_sm_get_state(btif_avk_cb[index].sm_handle);
      BTIF_TRACE_DEBUG("%s: AV State = %d", __func__, av_state);
      /* If remote device updates Codec configuration then btif_a2dp_sink_update_decoder
       * will be invoked depending on in which state updated SET_CONFIGURATION is received
       * AV STATE:
       *    STARTED/OPENED - Delay updating config so that previous configurartin is cleared
       *                     by the time we update new configuaration.
       *    OPENING/IDLE   - Run btif_a2dp_sink_update_decoder in AV State Machine Context
       *                     so that it is scheduled after previous configuarion is cleared.
       */
      if (av_state == BTIF_AVK_STATE_STARTED || av_state == BTIF_AVK_STATE_OPENED) {
        config_alarm = alarm_new("btif.a2dp_sink_set_config");
        if (config_alarm != NULL) {
          alarm_set(config_alarm, BTIF_DELAYED_UPDATE_DECODER_MS,
             (alarm_callback_t)btif_a2dp_sink_update_decoder,
             (uint8_t*)(p_data->avk_config.codec_info));
        } else {
          btif_a2dp_sink_update_decoder((uint8_t*)(p_data->avk_config.codec_info));
        }
      } else {
        btif_transfer_context(btif_avk_handle_event, BTA_AVK_SINK_MEDIA_CFG_EVT,
            (char *)p_data, sizeof(tBTA_AVK_SINK_MEDIA), NULL);
      }
      /* Switch to BTIF context */
      config_req.sample_rate =
          A2DP_GetTrackSampleRate(p_data->avk_config.codec_info);
      if (config_req.sample_rate == -1) {
        APPL_TRACE_ERROR("%s: cannot get the track frequency", __func__);
        break;
      }
      config_req.channel_count =
          A2DP_GetTrackChannelCount(p_data->avk_config.codec_info);
      if (config_req.channel_count == -1) {
        APPL_TRACE_ERROR("%s: cannot get the channel count", __func__);
        break;
      }

      config_req.peer_bd = p_data->avk_config.bd_addr;
      btif_transfer_context(btif_avk_handle_event, BTIF_AVK_SINK_CONFIG_REQ_EVT,
                            (char*)&config_req, sizeof(config_req), NULL);
      break;
    }
    default:
      break;
  }
}

/*******************************************************************************
 *
 * Function         btif_avk_init
 *
 * Description      Initializes btif AV if not already done
 *
 * Returns          bt_status_t
 *
 ******************************************************************************/

bt_status_t btif_avk_init(int service_id) {
  if (btif_avk_cb[0].sm_handle == NULL) {
    alarm_free(avk_open_on_rc_timer);
    avk_open_on_rc_timer = alarm_new("btif_av.avk_open_on_rc_timer");

    BTIF_TRACE_DEBUG("%s; service Id: %d", __func__, service_id);

    btif_avk_cb[0].service = service_id;
    btif_max_avk_clients = btif_avk_get_max_allowable_sink_connections();
    if (!btif_a2dp_sink_startup())
        return BT_STATUS_FAIL;  // Already running

    /* Also initialize the AV state machine */
    for (int i = 0; i < btif_max_avk_clients; i++)
      btif_avk_cb[i].sm_handle = btif_sm_init(
        (const btif_sm_handler_t*)btif_avk_state_handlers, BTIF_AVK_STATE_IDLE,
        i);

    btif_transfer_context(btif_avk_handle_event, BTIF_AVK_INIT_REQ_EVT,
        (char*)&service_id, sizeof(int), NULL);

    btif_enable_service(service_id);
  }

  return BT_STATUS_SUCCESS;
}



/*******************************************************************************
 *
 * Function         init_sink
 *
 * Description      Initializes the AV interface for sink mode
 *
 * Returns          bt_status_t
 *
 ******************************************************************************/

static bt_status_t init_sink(btav_sink_callbacks_t* callbacks) {
  BTIF_TRACE_EVENT("%s", __func__);

  bt_status_t status = btif_avk_init(BTA_A2DP_SINK_SERVICE_ID);
  if (status == BT_STATUS_SUCCESS) bt_avk_sink_callbacks = callbacks;

  return status;
}

/*******************************************************************************
 *
 * Function         update_audio_focus_state
 *
 * Description      Updates the final focus state reported by components calling
 *                  this module.
 *
 * Returns          None
 *
 ******************************************************************************/
static void update_audio_focus_state(int state) {
  BTIF_TRACE_DEBUG("%s: state=%d", __func__, state);
  btif_a2dp_sink_set_focus_state_req((btif_a2dp_sink_focus_state_t)state);
}

/*******************************************************************************
 *
 * Function         update_audio_track_gain
 *
 * Description      Updates the track gain (used for ducking).
 *
 * Returns          None
 *
 ******************************************************************************/
static void update_audio_track_gain(float gain) {
  BTIF_TRACE_DEBUG("%s: gain=%f", __func__, gain);
  btif_a2dp_sink_set_audio_track_gain(gain);
}


/*******************************************************************************
 *
 * Function         btif_get_latest_playing_device
 *
 * Description      Get the index for the most recent source
 *
 * Returns          None
 *
 ******************************************************************************/
void btif_avk_get_latest_playing_device(RawAddress *address) {
  int index;
  index = btif_avk_get_latest_playing_device_idx();
  if (index < btif_max_avk_clients)
        //copy bdaddrsss
    *address = btif_avk_cb[index].peer_bda;
  else
    *address = RawAddress::kEmpty;
}


/*******************************************************************************
 *
 * Function         btif_avk_is_device_connected
 *
 * Description      Check if the peer is connected(A2DP)
 *
 * Returns          true/false
 *
 ******************************************************************************/
bool btif_avk_is_device_connected(RawAddress address) {
  btif_sm_state_t state = btif_avk_get_conn_state_of_device(address);

  if ((state == BTIF_AVK_STATE_OPENED) ||
      (state == BTIF_AVK_STATE_STARTED))
    return true;
  else
    return false;
}

/*******************************************************************************
 *
 * Function         btif_avk_trigger_suspend
 *
 * Description      Trigger suspend when multicast is ongoing for tuch tones
 *                  and new ACL is created.
 *
 * Returns          void
 *
 ******************************************************************************/
void btif_avk_trigger_suspend(void) {
  int index;
  /*Get the current playing device*/
  BTIF_TRACE_DEBUG("%s()", __func__);
  index = btif_avk_get_latest_playing_device_idx();
  if (index <= btif_max_avk_clients) {
    /* Initiate SUSPEND for this device */
    BTIF_TRACE_DEBUG("Initiate SUSPEND for this device on index = %d", index);
    btif_sm_dispatch(btif_avk_cb[index].sm_handle, BTIF_AVK_SUSPEND_STREAM_REQ_EVT, NULL);
  } else
    BTIF_TRACE_ERROR("suspend on invalid index");
}


/*******************************************************************************
 *
 * Function         connect_int
 *
 * Description      Establishes the AV signalling channel with the remote
 *                  headset
 *
 * Returns          bt_status_t
 *
 ******************************************************************************/
static bt_status_t connect_int(RawAddress* bd_addr, uint16_t uuid) {
  btif_avk_connect_req_t connect_req;
  int i;
  connect_req.target_bda = bd_addr;
  connect_req.uuid = uuid;
  BTIF_TRACE_EVENT("%s", __func__);

  if (btif_storage_is_device_bonded(bd_addr) != BT_STATUS_SUCCESS)
  {
    BTIF_TRACE_WARNING("%s()## connect_int ## Device Not Bonded %s \n", __func__,
                      bd_addr->ToString().c_str());
    /* inform the application of the disconnection as the connection is not processed */
    btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED, bd_addr);
    btif_queue_advance_by_uuid(uuid, bd_addr);
    return BT_STATUS_SUCCESS;
  }
  for (i = 0; i < btif_max_avk_clients;) {
    if (btif_avk_get_valid_idx(i)) {
      if (*bd_addr == btif_avk_cb[i].peer_bda) {
        BTIF_TRACE_ERROR("Attempting connection for non idle device.. back off ");
        btif_queue_advance_by_uuid(uuid, bd_addr);
        return BT_STATUS_SUCCESS;
      }
      i++;
    } else
      break;
  }
  if (i == btif_max_avk_clients) {
    uint8_t rc_handle;

    BTIF_TRACE_ERROR("%s: All indexes are full", __func__);

    btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED, bd_addr);

    /* Multicast: Check if AV slot is available for connection
     * If not available, AV got connected to different devices.
     * Disconnect this RC connection without AV connection.
     */
    rc_handle = btif_avk_rc_get_connected_peer_handle(bd_addr->address);
    if (rc_handle != BTRC_HANDLE_NONE)
      BTA_AvkCloseRc(rc_handle);
    btif_queue_advance_by_uuid(uuid, bd_addr);
    return BT_STATUS_FAIL;
  }

  btif_sm_dispatch(btif_avk_cb[i].sm_handle, BTIF_AVK_CONNECT_REQ_EVT, (char*)&connect_req);

  return BT_STATUS_SUCCESS;
}


static bt_status_t sink_connect_src(const RawAddress& bd_addr) {
  BTIF_TRACE_EVENT("%s", __func__);
  CHECK_BTAV_INIT();

  return btif_queue_connect(UUID_SERVCLASS_AUDIO_SINK, bd_addr, connect_int,
                    btif_max_avk_clients);
}

/*******************************************************************************
 *
 * Function         disconnect
 *
 * Description      Tears down the AV signalling channel with the remote headset
 *
 * Returns          bt_status_t
 *
 ******************************************************************************/
static bt_status_t disconnect(const RawAddress& bd_addr) {
  BTIF_TRACE_EVENT("%s", __func__);
  CHECK_BTAV_INIT();

  /* Switch to BTIF context */
  return btif_transfer_context(btif_avk_handle_event, BTIF_AVK_DISCONNECT_REQ_EVT,
                               (char *)&bd_addr, sizeof(RawAddress), NULL);
}

/*******************************************************************************
 *
 * Function         cleanup
 *
 * Description      Shuts down the AV interface and does the cleanup
 *
 * Returns          None
 *
 ******************************************************************************/
static void cleanup(int service_uuid) {
  BTIF_TRACE_EVENT("%s", __func__);

  btif_transfer_context(btif_avk_handle_event, BTIF_AVK_CLEANUP_REQ_EVT,
                        (char*)&service_uuid, sizeof(int), NULL);

  btif_disable_service(service_uuid);

  btif_disable_service(BTA_TWS_PLUS_SERVICE_ID);

  alarm_free(avk_open_on_rc_timer);
  avk_open_on_rc_timer = NULL;
}

static void cleanup_sink(void) {
  BTIF_TRACE_EVENT("%s:", __func__);

  btif_queue_cleanup(UUID_SERVCLASS_AUDIO_SINK);
 // if (bt_av_src_callbacks == NULL)  //TODO cleanup sink
      cleanup(BTA_A2DP_SINK_SERVICE_ID);
}


static const btav_sink_interface_t bt_avk_sink_interface = {
    sizeof(btav_sink_interface_t),
    init_sink,
    sink_connect_src,
    disconnect,
    cleanup_sink,
    update_audio_focus_state,
    update_audio_track_gain,
};

RawAddress btif_avk_get_addr_by_index(int idx) {
  return (idx < btif_max_avk_clients) ? btif_avk_cb[idx].peer_bda:RawAddress::kEmpty;
}

/*******************************************************************************
 *
 * Function         btif_avk_get_addr
 *
 * Description      Fetches current AV BD address
 *
 * Returns          BD address
 *
 ******************************************************************************/
RawAddress btif_avk_get_addr(RawAddress address) { 
  int i;
  RawAddress not_found = RawAddress::kEmpty;
  for (i = 0; i < btif_max_avk_clients; i++)
    if (btif_avk_cb[i].peer_bda == address)
      return btif_avk_cb[i].peer_bda;
  return not_found;
}

/******************************************************************************
 *
 * Function         btif_avk_get_peer_addr
 *
 * Description      Returns peer device address
 *
 * Returns          void. Updates the peer_bda argument
 *******************************************************************************/
void btif_avk_get_peer_addr(RawAddress* peer_bda) {
  btif_sm_state_t state = BTIF_AVK_STATE_IDLE;
  for (int i = 0; i < btif_max_avk_clients; i++) {
    state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
    if ((state == BTIF_AVK_STATE_OPENED) ||
        (state == BTIF_AVK_STATE_STARTED)) {
      BTIF_TRACE_DEBUG("btif_avk_get_peer_addr: %u state: %d ",
              btif_avk_cb[i].peer_bda, state);
      *peer_bda = btif_avk_cb[i].peer_bda;
      if (state == BTIF_AVK_STATE_STARTED)
        break;
    }
  }
}

/*******************************************************************************
 * Function         btif_avk_is_sink_enabled
 *
 * Description      Checks if A2DP Sink is enabled or not
 *
 * Returns          true if A2DP Sink is enabled, false otherwise
 *
 ******************************************************************************/

bool btif_avk_is_sink_enabled(void) {
  return (bt_avk_sink_callbacks != NULL) ? true : false;
}

/*******************************************************************************
 *
 * Function         btif_avk_stream_ready
 *
 * Description      Checks whether AV is ready for starting a stream
 *
 * Returns          None
 *
 ******************************************************************************/
bool btif_avk_stream_ready(void) {
  int i;
  bool status = false;

  /* also make sure main adapter is enabled */
  if (btif_is_enabled() == 0) {
    BTIF_TRACE_EVENT("%s: main adapter not enabled", __func__);
    return false;
  }

  for (i = 0; i < btif_max_avk_clients; i++) {
    btif_avk_cb[i].state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
    BTIF_TRACE_DEBUG("%s : sm hdl %d, state %d, flags %x, handoff %d", __func__,
                   btif_avk_cb[i].sm_handle, btif_avk_cb[i].state,
                   btif_avk_cb[i].flags, btif_avk_cb[i].dual_handoff);
    /* Multicast:
     * If any of the stream is in pending suspend state when
     * we initiate start, it will result in inconsistent behavior
     * Check the pending SUSPEND flag and return failure
     * if suspend is in progress.
     */
    if (btif_avk_cb[i].dual_handoff ||
        (btif_avk_cb[i].flags & BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING)) {
      status = false;
      break;
    } else if (btif_avk_cb[i].flags &
        (BTIF_AVK_FLAG_REMOTE_SUSPEND|BTIF_AVK_FLAG_PENDING_STOP|BTIF_AVK_FLAG_PENDING_DISCONNECT)) {
      status = false;
      break;
    } else if (btif_avk_cb[i].state == BTIF_AVK_STATE_OPENED)
      status = true;
  }
  BTIF_TRACE_DEBUG("%s(): %d", __func__, status);
  return status;
}


/*******************************************************************************
 *
 * Function         btif_avk_clear_remote_start_timer
 *
 * Description      Clear latest av start timer
 *
 * Returns          bool
 *
 ******************************************************************************/
void  btif_avk_clear_remote_start_timer(int index) {
  if(index < btif_max_avk_clients && index >= 0) {
    btif_avk_cb[index].remote_started = false;
    if(btif_avk_cb[index].remote_start_alarm != NULL)
      alarm_free(btif_avk_cb[index].remote_start_alarm);
    btif_avk_cb[index].remote_start_alarm = NULL;
  }
}


/*******************************************************************************
 *
 * Function         btif_avk_stream_started_ready
 *
 * Description      Checks whether AV ready for media start in streaming state
 *
 * Returns          bool
 *
 ******************************************************************************/

bool btif_avk_stream_started_ready(void)
{
  int i;
  bool status = false;;

  for (i = 0; i < btif_max_avk_clients; i++) {
    btif_avk_cb[i].state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
    if (btif_avk_cb[i].dual_handoff) {
      BTIF_TRACE_ERROR("%s: Under Dual handoff ",__func__ );
      status = false;;
      break;
    } else if (btif_avk_cb[i].flags &
               (BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING |
                BTIF_AVK_FLAG_REMOTE_SUSPEND |
                BTIF_AVK_FLAG_PENDING_STOP)) {
      status = false;;
      break;
    } else if (btif_avk_cb[i].state == BTIF_AVK_STATE_STARTED)
      status = true;
  }
  BTIF_TRACE_DEBUG("btif_avk_stream_started_ready: %d", status);
  return status;
}

/*******************************************************************************
**
** Function         btif_avk_is_start_ack_pending
**
** Description      Checks whether start command is sent but not acked by remote
**
** Returns          None
**
*******************************************************************************/

bool btif_avk_is_start_ack_pending(void)
{
    int i;
    bool status = false;

    for (i = 0; i < btif_max_avk_clients; i++)
    {
        if (btif_avk_cb[i].flags & BTIF_AVK_FLAG_PENDING_START)
        {
            status = true;
            break;
        }
    }
    BTIF_TRACE_DEBUG("btif_avk_is_start_ack_pending: %d", status);
    return status;
}

/*******************************************************************************
**
** Function         btif_avk_is_suspend_stop_pending_ack
**
** Description      Checks whether suspend/stop is pending ack when state is started
**
** Returns          None
**
*******************************************************************************/
bool btif_avk_is_suspend_stop_pending_ack(void)
{
    int i;
    bool status = FALSE;

    for (i = 0; i < btif_max_avk_clients; i++)
    {
        btif_avk_cb[i].state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
        BTIF_TRACE_DEBUG("btif_avk_stream_ready flags: %d, state: %d",
                                btif_avk_cb[i].flags, btif_avk_cb[i].state);
        if ((btif_avk_cb[i].flags &
            (BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING |
            BTIF_AVK_FLAG_PENDING_STOP)) &&
            (btif_avk_cb[i].state == BTIF_AVK_STATE_STARTED)) {
          status = true;
          break;
        }
    }
    BTIF_TRACE_DEBUG("btif_av_is_stream_suspend_pending_ack: %d", status);
    return status;
}

/*******************************************************************************
 *
 * Function         btif_avk_dispatch_sm_event
 *
 * Description      Send event to AV statemachine
 *
 * Returns          None
 *
 ******************************************************************************/

/* used to pass events to AV statemachine from other tasks */
void btif_avk_dispatch_sm_event(btif_avk_sm_event_t event, void* p_data, int len) {
  /* Switch to BTIF context */
  BTIF_TRACE_IMP("%s: event: %d, len: %d", __FUNCTION__, event, len);
  btif_transfer_context(btif_avk_handle_event, event, (char*)p_data, len, NULL);
  BTIF_TRACE_IMP("%s: event %d sent", __FUNCTION__, event);
}

/*******************************************************************************
 *
 * Function         btif_avk_sink_execute_service
 *
 * Description      Initializes/Shuts down the service
 *
 * Returns          BT_STATUS_SUCCESS on success, BT_STATUS_FAIL otherwise
 *
 ******************************************************************************/
bt_status_t btif_avk_sink_execute_service(bool b_enable) {
  int i;
  BTIF_TRACE_DEBUG("%s(): enable: %d", __func__, b_enable);

  bool delay_report_enabled = false;
  char value[PROPERTY_VALUE_MAX] = {'\0'};
  tBTA_AVK_FEAT feat_delay_rpt = 0;

  if (b_enable) {
    /* Added BTA_AVK_FEAT_NO_SCO_SSPD - this ensures that the BTA does not
     * auto-suspend av streaming on AG events(SCO or Call). The suspend shall
     * be initiated by the app/audioflinger layers
     */

    osi_property_get("persist.vendor.bt.a2dp.delay_report_sink", value, "false");
    delay_report_enabled = (strcmp(value, "true") == 0);
    BTIF_TRACE_DEBUG("%s: delay_report_enabled = %d", __func__, delay_report_enabled);
    if (delay_report_enabled)
      feat_delay_rpt = BTA_AVK_FEAT_DELAY_RPT;

    BTA_AvkEnable(BTA_SEC_AUTHENTICATE, BTA_AVK_FEAT_NO_SCO_SSPD|BTA_AVK_FEAT_RCCT|
                                        BTA_AVK_FEAT_METADATA|BTA_AVK_FEAT_VENDOR|
                                        BTA_AVK_FEAT_ADV_CTRL|BTA_AVK_FEAT_RCTG|feat_delay_rpt,
                                        bte_avk_callback);

    BTA_AvkUpdateMaxAVClient(btif_max_avk_clients);
    BTIF_TRACE_ERROR("%s: btif_max_avk_clients= %d", __func__, btif_max_avk_clients);
    for (i = 0; i < btif_max_avk_clients; i++) {
        BTIF_TRACE_ERROR("%s: BTA_AvkRegister= %d", __func__, i);
        BTA_AvkRegister(BTA_AVK_CHNL_AUDIO, BTIF_AVK_SERVICE_NAME, 0,
                bte_avk_sink_media_callback, UUID_SERVCLASS_AUDIO_SINK);
    }
  } else {
    /* Also shut down the AV state machine */
    for (i = 0; i < btif_max_avk_clients; i++ ) {
      if (btif_avk_cb[i].sm_handle != NULL) {
        BTIF_TRACE_DEBUG("%s(): shutting down AV SM", __func__);
        btif_sm_shutdown(btif_avk_cb[i].sm_handle);
        btif_avk_cb[i].sm_handle = NULL;
      }
    }
    BTA_AvkDeregister(btif_avk_cb[0].bta_handle);
    BTA_AvkDisable();
  }

  return BT_STATUS_SUCCESS;
}


/*******************************************************************************
 *
 * Function         btif_avk_get_sink_interface
 *
 * Description      Get the AV callback interface for A2DP sink profile
 *
 * Returns          btav_sink_interface_t
 *
 ******************************************************************************/
const btav_sink_interface_t* btif_avk_get_sink_interface(void) {
  BTIF_TRACE_EVENT("%s", __func__);
  return &bt_avk_sink_interface;
}

/*******************************************************************************
 *
 * Function         btif_avk_is_connected
 *
 * Description      Checks if av has a connected sink
 *
 * Returns          bool
 *
 ******************************************************************************/
bool btif_avk_is_connected(void) {
  int i;
  bool status = false;
  for (i = 0; i < btif_max_avk_clients; i++) {
    btif_avk_cb[i].state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
    if ((btif_avk_cb[i].state == BTIF_AVK_STATE_OPENED) ||
        (btif_avk_cb[i].state ==  BTIF_AVK_STATE_STARTED)) {
      status = true;
      break;
    }
  }
  return status;
}



/*******************************************************************************
**
** Function         btif_avk_is_connected_on_other_idx
**
** Description      Checks if any other AV SCB is connected
**
** Returns          bool
**
*******************************************************************************/
bool btif_avk_is_connected_on_other_idx(int current_index) {
  //return true if other IDx is connected
  btif_sm_state_t state = BTIF_AVK_STATE_IDLE;
  int i;
  for (i = 0; i < btif_max_avk_clients; i++) {
    if (i != current_index) {
      state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
      if ((state == BTIF_AVK_STATE_OPENED) ||
          (state == BTIF_AVK_STATE_STARTED))
        return true;
    }
  }
  return false;
}

/*******************************************************************************
**
** Function         btif_avk_get_other_connected_idx
**
** Description      Checks if any AV SCB is connected other than the current
**                  index
**
** Returns          index
**
*******************************************************************************/
int btif_avk_get_other_connected_idx(int current_index)
{
  btif_sm_state_t state = BTIF_AVK_STATE_IDLE;
  int i;
  // return true if other IDx is connected
  for (i = 0; i < btif_max_avk_clients; i++) {
    if (i != current_index) {
      state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
      if ((state == BTIF_AVK_STATE_OPENED) ||
          (state == BTIF_AVK_STATE_STARTED))
        return i;
    }
  }
  return INVALID_INDEX;
}

/*******************************************************************************
**
** Function         btif_avk_is_playing_on_other_idx
**
** Description      Checks if any other AV SCB is connected
**
** Returns          bool
**
*******************************************************************************/
bool btif_avk_is_playing_on_other_idx(int current_index)
{
  btif_sm_state_t state = BTIF_AVK_STATE_IDLE;
  int i;
  // return true if other IDx is playing
  for (i = 0; i < btif_max_avk_clients; i++) {
    if (i != current_index) {
      state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
      if (state == BTIF_AVK_STATE_STARTED)
        return true;
    }
  }
  return false;
}

/*******************************************************************************
**
** Function         btif_avk_is_local_started_on_other_idx
**
** Description      Checks if any other AV SCB is connected and
**                  remote started is not set
**
** Returns          bool
**
*******************************************************************************/
bool btif_avk_is_local_started_on_other_idx(int current_index)
{
  btif_sm_state_t state = BTIF_AVK_STATE_IDLE;
  int i;
  // return true if other IDx is playing
  for (i = 0; i < btif_max_avk_clients; i++) {
    if (i != current_index) {
      state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
      if (state == BTIF_AVK_STATE_STARTED && !btif_avk_cb[i].remote_started)
        return true;
    }
  }
  return false;
}


/*******************************************************************************
**
** Function         btif_avk_is_remote_started_set
**
** Description      Checks if remote started is not set for particular index
**
** Returns          bool
**
*******************************************************************************/
bool btif_avk_is_remote_started_set(int index)
{
  if(index != INVALID_INDEX && index < btif_max_avk_clients)
    return btif_avk_cb[index].remote_started;
  return false;
}


/*******************************************************************************
 *
 * Function         btif_avk_get_current_playing_dev_idx
 *
 * Description      get current playing device index
 *
 * Returns          int
 *
 ******************************************************************************/
int btif_avk_get_current_playing_dev_idx(void)
{
  int i;

  for (i = 0; i < btif_max_avk_clients; i++) {
    if (btif_avk_cb[i].current_playing == TRUE) {
      BTIF_TRACE_DEBUG("current playing on index = %d", i);
      return i;
    }
  }

  return 0;
}

/******************************************************************************
**
** Function        btif_avk_get_sink_latency
**
** Description     get initial sink latency
**
** Returns         tBTA_AVK_LATENCY
**
********************************************************************************/
tBTA_AVK_LATENCY btif_avk_get_sink_latency() {
  int index = 0;
  int i;
  tBTA_AVK_LATENCY latency = 0;
  tBTA_AVK_LATENCY sink_latency = 0;

  if (enable_multicast == false) {
    index = btif_avk_get_current_playing_dev_idx();
    /* Set sink latency to be latency reported from sink if delay reporting
     * is enabled, or else set sink latency to be default sink latency value */
    if (btif_avk_cb[index].sink_latency > 0)
      sink_latency = btif_avk_cb[index].sink_latency;
    else
      sink_latency = BTIF_AVK_DEFAULT_SINK_LATENCY;

  } else {
    BTIF_TRACE_DEBUG("%s, multicast enabled, calculate average sink latency", __func__);
    for (i = 0; i < btif_max_avk_clients; i++) {
      if (btif_avk_cb[i].sink_latency > 0)
        latency += btif_avk_cb[i].sink_latency;
      else
        latency += BTIF_AVK_DEFAULT_MULTICAST_SINK_LATENCY;
    }
    if (latency > 0)
      sink_latency = latency / btif_max_avk_clients;
    else
      sink_latency = BTIF_AVK_DEFAULT_MULTICAST_SINK_LATENCY;
  }

  BTIF_TRACE_DEBUG("%s, return sink latency: %d", __func__, sink_latency);
  return sink_latency;
}


/******************************************************************************
**
** Function        btif_avk_is_scrambling_enabled
**
** Description     get scrambling is enabled from bluetooth.
**
** Returns         bool
**
********************************************************************************/
bool btif_avk_is_scrambling_enabled() {
  uint8_t no_of_freqs = 0;
  uint8_t *freqs = NULL;
  btav_a2dp_codec_config_t codec_config;
  std::vector<btav_a2dp_codec_config_t> codecs_local_capabilities;
  std::vector<btav_a2dp_codec_config_t> codecs_selectable_capabilities;
  A2dpCodecs* a2dp_codecs = bta_avk_get_a2dp_codecs();
  if (a2dp_codecs == nullptr) return false;

  if (!a2dp_codecs->getCodecConfigAndCapabilities(
          &codec_config, &codecs_local_capabilities,
          &codecs_selectable_capabilities)) {
    BTIF_TRACE_WARNING(
        "btif_avk_is_scrambling_enabled failed: "
        "cannot get codec config and capabilities");
    return false;
  }
  freqs = controller_get_interface()->get_scrambling_supported_freqs(&no_of_freqs);
  if(no_of_freqs == 0) {
    BTIF_TRACE_WARNING(
        "BT controller doesn't support scrambling");
    return false;
  }

  if (freqs != NULL) {
    for ( uint8_t i = 0; i < no_of_freqs; i++) {
      if (freqs[i] ==  ( uint8_t ) codec_config.sample_rate ) {
         return true;
      }
    }
  }
  return false;
}

/******************************************************************************
**
** Function        btif_avk_is_44p1kFreq_supported
**
** Description     get 44p1kFreq is enabled from bluetooth.
**
** Returns         bool
**
********************************************************************************/
bool btif_avk_is_44p1kFreq_supported() {
  uint8_t add_on_features_size = 0;
  const bt_device_features_t * add_on_features_list = NULL;

  add_on_features_list = controller_get_interface()->get_add_on_features(&add_on_features_size);
  if (add_on_features_size == 0) {
    BTIF_TRACE_WARNING(
        "BT controller doesn't add on features");
    return false;
  }

  if (add_on_features_list != NULL) {
    if (HCI_SPLIT_A2DP_44P1KHZ_SAMPLE_FREQ(add_on_features_list->as_array)) {
      return true;
    }
  }
  return false;
}

uint8_t btif_avk_get_peer_sep() {
  if (isA2dpSink == true)
    return AVDT_TSEP_SNK;
  else
    return AVDT_TSEP_SRC;
}

/*******************************************************************************
 *
 * Function         btif_avk_is_peer_edr
 *
 * Description      Check if the connected a2dp device supports
 *                  EDR or not. Only when connected this function
 *                  will accurately provide a true capability of
 *                  remote peer. If not connected it will always be false.
 *
 * Returns          true if remote device is capable of EDR
 *
 ******************************************************************************/
bool btif_avk_is_peer_edr(void) {
  btif_sm_state_t state;
  bool peer_edr = false;

  ASSERTC(btif_avk_is_connected(), "No active a2dp connection", 0);

  /* If any of the remote in streaming state is BR
   * return false to ensure proper configuration
   * is used. Ideally, since multicast is not supported
   * if any of the connected device is BR device,
   * we should not see both devices in START state.
   */
  for (int index = 0; index < btif_max_avk_clients; index ++) {
    state = btif_sm_get_state(btif_avk_cb[index].sm_handle);
    if ((btif_avk_cb[index].flags & BTIF_AVK_FLAG_PENDING_START)
            || btif_avk_cb[index].current_playing) {
      if (btif_avk_cb[index].edr)
        peer_edr = true;
      else
        return false;
    }
  }
  return peer_edr;
}

/*******************************************************************************
 *
 * Function         btif_avk_any_br_peer
 *
 * Description      Check if the any of connected devices is BR device.
 *
 * Returns          true if connected to any BR device, false otherwise.
 *
 ******************************************************************************/
bool btif_avk_any_br_peer(void) {
  btif_sm_state_t state;
  for (int index = 0; index < btif_max_avk_clients; index ++) {
    state = btif_sm_get_state(btif_avk_cb[index].sm_handle);
    if (state >= BTIF_AVK_STATE_OPENED) {
      if (!btif_avk_cb[index].edr) {
        BTIF_TRACE_WARNING("%s : Connected to BR device :", __FUNCTION__);
        return true;
      }
    }
  }
  return false;
}

/*******************************************************************************
 *
 * Function         btif_avk_peer_supports_3mbps
 *
 * Description      check if the connected a2dp device supports
 *                  3mbps edr. Only when connected this function
 *                  will accurately provide a true capability of
 *                  remote peer. If not connected it will always be false.
 *
 * Returns          true if remote device is EDR and supports 3mbps
 *
 ******************************************************************************/
bool btif_avk_peer_supports_3mbps(void) {
  btif_sm_state_t state;
  ASSERTC(btif_avk_is_connected(), "No active a2dp connection", 0);

  for (int index = 0; index < btif_max_avk_clients; index ++) {
    state = btif_sm_get_state(btif_avk_cb[index].sm_handle);
    if ((btif_avk_cb[index].flags & BTIF_AVK_FLAG_PENDING_START)
         || btif_avk_cb[index].current_playing)
      if (btif_avk_cb[index].edr_3mbps)
        return true;
  }
  return false;
}

/******************************************************************************
 *
 * Function        btif_avk_clear_remote_suspend_flag
 *
 * Description     Clears btif_av_cd.flags if BTIF_AVK_FLAG_REMOTE_SUSPEND is set
 *
 * Returns          void
 *****************************************************************************/
void btif_avk_clear_remote_suspend_flag(void) {
  int i;
  for (i = 0; i < btif_max_avk_clients; i++) {
    BTIF_TRACE_DEBUG("%s(): flag :%x", __func__, btif_avk_cb[i].flags);
    btif_avk_cb[i].flags  &= ~BTIF_AVK_FLAG_REMOTE_SUSPEND;
    bta_avk_sniff_enable(true, btif_avk_cb[i].peer_bda);
  }
}

/*******************************************************************************
 *
 * Function         btif_avk_move_idle
 *
 * Description      Opening state is intermediate state. It cannot handle
 *                  incoming/outgoing connect/disconnect requests.When ACL
 *                  is disconnected and we are in opening state then move back
 *                  to idle state which is proper to handle connections.
 *
 * Returns          Void
 *
 ******************************************************************************/
void btif_avk_move_idle(RawAddress bd_addr) {
  int index =0;
  /* inform the application that ACL is disconnected and move to idle state */
  index = btif_avk_idx_by_bdaddr(&bd_addr);
  if (index == btif_max_avk_clients) {
    BTIF_TRACE_IMP("btif_avk_move_idle: Already in IDLE");
    return;
  }
  btif_sm_state_t state = btif_sm_get_state(btif_avk_cb[index].sm_handle);
  BTIF_TRACE_IMP("ACL Disconnected state %d  is same device %d",state,
           memcmp (&bd_addr, &(btif_avk_cb[index].peer_bda), sizeof(bd_addr)));
  if (state == BTIF_AVK_STATE_OPENING &&
      (memcmp (&bd_addr, &(btif_avk_cb[index].peer_bda), sizeof(bd_addr)) == 0)) {
    BTIF_TRACE_IMP("Moving BTIF State from Opening to Idle due to ACL disconnect");
    btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED, &(btif_avk_cb[index].peer_bda));
    BTA_AvkClose(btif_avk_cb[index].bta_handle);
    btif_sm_change_state(btif_avk_cb[index].sm_handle, BTIF_AVK_STATE_IDLE);
    btif_queue_advance_by_uuid(UUID_SERVCLASS_AUDIO_SINK, &(btif_avk_cb[index].peer_bda));
    btif_report_connection_state_to_ba(BTAV_CONNECTION_STATE_DISCONNECTED);
  }
}

/******************************************************************************
 *
 * Function        btif_avk_get_num_playing_devices
 *
 * Description     Return number of A2dp playing devices
 *
 * Returns         int
 *****************************************************************************/
uint16_t btif_avk_get_num_playing_devices(void) {
  uint16_t i;
  uint16_t playing_devices = 0;
  for (i = 0; i < btif_max_avk_clients; i++) {
    btif_avk_cb[i].state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
    if (btif_avk_cb[i].state ==  BTIF_AVK_STATE_STARTED)
        playing_devices++;
  }
  BTIF_TRACE_DEBUG("AV devices playing: %d", playing_devices);

  return playing_devices;
}
/*******************************************************************************
 *
 * Function        btif_avk_get_num_connected_devices
 *
 * Description     Return number of A2dp connected devices
 *
 * Returns         int
 *****************************************************************************/
uint16_t btif_avk_get_num_connected_devices(void) {
  uint16_t i;
  uint16_t connected_devies = 0;
  for (i = 0; i < btif_max_avk_clients; i++)
  {
    btif_avk_cb[i].state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
    if ((btif_avk_cb[i].state == BTIF_AVK_STATE_OPENED) ||
        (btif_avk_cb[i].state ==  BTIF_AVK_STATE_STARTED))
      connected_devies++;
  }
  BTIF_TRACE_DEBUG("AV Connection count: %d", connected_devies);

  return connected_devies;
}

/******************************************************************************
**
** Function         btif_avk_get_av_hdl_from_idx
**
** Description      Returns bta handle from the device index
**
** Returns         BTA HANDLE
********************************************************************************/
tBTA_AVK_HNDL btif_avk_get_av_hdl_from_idx(int idx)
{
    if (idx == btif_max_avk_clients)
    {
        BTIF_TRACE_ERROR("%s: Invalid handle",__func__);
        return -1;
    }
    return btif_avk_cb[idx].bta_handle;
}

#if (TWS_ENABLED == TRUE)
extern void btif_avk_set_earbud_state(const RawAddress& address, uint8_t earbud_state);
extern void btif_avk_set_earbud_role(const RawAddress& address, uint8_t earbud_role);
void btif_avk_set_earbud_state(const RawAddress& address, uint8_t earbud_state) {
  BTIF_TRACE_EVENT("btif_avk_set_earbud_state = %d", earbud_state);
}
void btif_avk_set_earbud_role(const RawAddress& address, uint8_t earbud_role) {
  BTIF_TRACE_EVENT("btif_avk_set_earbud_role = %d",earbud_role);
  int index = btif_avk_idx_by_bdaddr(&(RawAddress&)address);
  if (index == btif_max_avk_clients) {
    BTIF_TRACE_ERROR("%s: invalid index",__func__);
    return;
  }
  BTA_AVSetEarbudRole(earbud_role, btif_avk_cb[index].bta_handle);
}
#endif

/******************************************************************************
**
** Function        btif_avk_get_multicast_state
**
** Description     Returns true if multicast is enabled else false
**
** Returns         bool
******************************************************************************/
bool btif_avk_get_multicast_state() {
  return enable_multicast;
}


/******************************************************************************
**
** Function        btif_avk_is_multicast_supported
**
** Description     Returns true if multicast is supported
**
** Returns         bool
******************************************************************************/
bool btif_avk_is_multicast_supported() {
  return false;
}

bool btif_avk_check_flag_remote_suspend(int index) {
  BTIF_TRACE_ERROR("%s(): index = %d",__func__,index);
  if (btif_avk_cb[index].flags & BTIF_AVK_FLAG_REMOTE_SUSPEND) {
    BTIF_TRACE_DEBUG("remote suspend flag set on index = %d",index);
    return true;
  } else {
    BTIF_TRACE_DEBUG("remote suspend flag not set on index = %d",index);
    return false;
  }
}

/* SPLITA2DP*/
/*******************************************************************************
 *
 * Function         btif_avk_is_split_a2dp_enabled
 *
 * Description      Check if split a2dp is enabled.
 *
 * Returns          TRUE if split a2dp is enabled, FALSE otherwise
 *
 ******************************************************************************/
bool btif_avk_is_split_a2dp_enabled() {
  BTIF_TRACE_DEBUG("btif_avk_is_split_a2dp_enabled:false");
  return false;
}

bool btif_avk_is_handoff_set() {
  for (int i = 0; i < btif_max_avk_clients; i++) {
    if (btif_avk_cb[i].dual_handoff) {
      BTIF_TRACE_DEBUG("%s: AV is under handoff for idx = %d",__func__, i);
      return true;
    }
  }
  return false;
}

bool btif_avk_is_device_disconnecting() {
  int i;
  btif_sm_state_t state = BTIF_AVK_STATE_IDLE;
  BTIF_TRACE_DEBUG("btif_avk_is_device_disconnecting");
  for (i = 0; i < btif_max_avk_clients; i++) {
    state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
    BTIF_TRACE_DEBUG("%s: state = %d",__func__,state);
    if ((btif_avk_cb[i].dual_handoff &&
      state == BTIF_AVK_STATE_CLOSING)) {
      BTIF_TRACE_DEBUG("Device disconnecting");
      return true;
    }
  }
  return false;
}
void btif_avk_reset_reconfig_flag() {
  int i;
  BTIF_TRACE_DEBUG("%s",__func__);
  reconfig_a2dp = FALSE;
  for (i = 0; i < btif_max_avk_clients; i++)
  {
    if (btif_avk_cb[i].dual_handoff) {
      BTIF_TRACE_DEBUG("%s resetting dual handoff flag for index %d",__func__, i);
      btif_avk_cb[i].dual_handoff = FALSE;
    }
  }
}

/******************************************************************************
**
** Function         btif_avk_get_reconfig_dev_hndl
**
** Description     Retrieve the bta handle for the device codec switch is
**                 triggered in the middle of streaming.
**
** Returns         BTA HANDLE if codec switch triggered during streaming
********************************************************************************/
tBTA_AVK_HNDL btif_avk_get_reconfig_dev_hndl() {
  int i = 0;
  BTIF_TRACE_DEBUG("%s",__func__);
  for (i = 0 ; i < btif_max_avk_clients; i++) {
    if (btif_avk_cb[i].reconfig_pending)
        return btif_avk_cb[i].bta_handle;
  }
  return 0;
}

/******************************************************************************
**
** Function        btif_avk_reset_codec_reconfig_flag
**
** Description     Reset codec reconfig flag if avdtp reconfig is not triggered
**                 for user provided codec config
**
** Returns         void
********************************************************************************/
void btif_avk_reset_codec_reconfig_flag(RawAddress address) {
  int i;
  BTIF_TRACE_DEBUG("%s",__func__);
  i = btif_avk_idx_by_bdaddr(&address);
  if (i < btif_max_avk_clients) {
    if (btif_avk_cb[i].reconfig_pending)
      btif_avk_cb[i].reconfig_pending = false;
  } else {
  /*clearing all reconfig_pending when address will not match */
    for (i = 0; i < btif_max_avk_clients; i++)
      if (btif_avk_cb[i].reconfig_pending)
        btif_avk_cb[i].reconfig_pending = false;
  }
}

/******************************************************************************
**
** Function        btif_avk_reinit_audio_interface
**
** Description     Reinit audio interface,this function is called when BT
**                 audio hal server is died
**
** Returns         void
********************************************************************************/
void btif_avk_reinit_audio_interface() {
  BTIF_TRACE_DEBUG(LOG_TAG,"btif_av_reint_audio_interface");
  btif_a2dp_audio_interface_init();
  btif_a2dp_audio_if_init = true;
}

void btif_avk_flow_spec_cmd(int index, int bitrate) {
  tBT_FLOW_SPEC flow_spec;
  memset(&flow_spec, 0x00, sizeof(flow_spec));
  flow_spec.flow_direction = 0x00;     /* flow direction - out going */
  flow_spec.service_type = 0x02;       /* Guaranteed */
  flow_spec.token_rate = 0x00;         /* bytes/second - no token rate is specified*/
  flow_spec.token_bucket_size = 0x00;  /* bytes - no token bucket is needed*/
  flow_spec.latency = 0xFFFFFFFF;      /* microseconds - default value */
  flow_spec.peak_bandwidth = bitrate/8;/*bytes per second */
  //Sending Flow_spec has been taken care whenever event:BTIF_AVK_START_STREAM_REQ_EVT comes.
  //BTM_FlowSpec (btif_avk_cb[index].peer_bda, &flow_spec, NULL);
  APPL_TRACE_DEBUG("%s peak_bandwidth %d",__func__, flow_spec.peak_bandwidth);
}
#if (TWS_ENABLED == TRUE)
bool btif_avk_is_tws_device_playing(int index) {
  int i;
  btif_sm_state_t state = BTIF_AVK_STATE_IDLE;
  for (i = 0; i < btif_max_avk_clients; i++) {
    state = btif_sm_get_state(btif_avk_cb[i].sm_handle);
    if (i != index && state == BTIF_AVK_STATE_STARTED) {
      if (btif_avk_cb[i].tws_device) {
        BTIF_TRACE_EVENT("btif_avk_is_tws_device_playing on index = %d",i);
        return true;
      }
    }
  }
  return false;
}
bool btif_avk_is_tws_suspend_triggered(int index) {
  for (int i = 0; i < btif_max_avk_clients; i++) {
    if (i != index && btif_avk_cb[i].tws_device) {
    //TODO check for tws pair
      if (btif_avk_cb[i].flags & BTIF_AVK_FLAG_LOCAL_SUSPEND_PENDING) {
        BTIF_TRACE_EVENT("Local suspend triggered on tws pair");
        return true;
      }
    }
  }
  return false;
}
bool btif_avk_is_tws_enabled_for_dev(const RawAddress& rc_addr) {
  for (int i = 0; i < btif_max_avk_clients; i++) {
    if (btif_avk_cb[i].peer_bda == rc_addr) {
      BTIF_TRACE_DEBUG("%s",__func__);
      return btif_avk_cb[i].tws_device;
    }
  }
  return false;
}
bool btif_avk_is_tws_connected() {
  for (int i = 0; i < btif_max_avk_clients; i++) {
    if (btif_avk_cb[i].tws_device) {
      BTIF_TRACE_DEBUG("%s",__func__);
      return true;
    }
  }
  return false;
}
bool btif_avk_current_device_is_tws() {
  for (int i = 0; i < btif_max_avk_clients; i++) {
    if (btif_avk_cb[i].tws_device && btif_avk_cb[i].current_playing) {
      BTIF_TRACE_DEBUG("%s",__func__);
      return true;
    }
  }
  return false;
}
bool btif_avk_is_idx_tws_device(int index) {
  if (index == btif_max_avk_clients) {
    BTIF_TRACE_ERROR("%s:invalid index",__func__);
    return false;
  }
  BTIF_TRACE_DEBUG("%s:%d",__func__, btif_avk_cb[index].tws_device);
  return btif_avk_cb[index].tws_device;
}
int btif_avk_get_tws_pair_idx(int index) {
  BTIF_TRACE_DEBUG("%s",__func__);
  int idx = btif_max_avk_clients;
  if (index == btif_max_avk_clients) {
    BTIF_TRACE_ERROR("%s:invalid index",__func__);
    return false;
  }
  for (idx = 0; idx < btif_max_avk_clients; idx++) {
    if (idx != index && btif_avk_cb[idx].tws_device) {
      BTIF_TRACE_DEBUG("%s:found TWS+ pair index %d",__func__,idx);
      return idx;
    }
  }
  return idx;
}

bool btif_avk_is_tws_enable_monocfg() {
  int i,index;
  BTIF_TRACE_DEBUG("%s",__func__);
  btif_sm_state_t state = BTIF_AVK_STATE_IDLE;
  RawAddress tws_address_peer;
  i = btif_avk_get_latest_playing_device_idx();
  if (btif_avk_cb[i].tws_device) {
    if (BTM_SecGetTwsPlusPeerDev(btif_avk_cb[i].peer_bda,tws_address_peer)== true) {
          index = btif_avk_idx_by_bdaddr(&tws_address_peer);
          if (index < btif_max_avk_clients) {
            state = btif_sm_get_state(btif_avk_cb[index].sm_handle);
            if (state == BTIF_AVK_STATE_STARTED || (btif_avk_cb[index].flags & BTIF_AVK_FLAG_PENDING_START))
               return false;
          }
       }
    }
  return true;
}
#endif
/*SPLITA2DP*/

bool btif_avk_is_state_opened(int i) {
  return (btif_sm_get_state(btif_avk_cb[i].sm_handle) == BTIF_AVK_STATE_OPENED);
}

void btif_avk_set_audio_delay(uint16_t delay, tBTA_AVK_HNDL hndl) {
  int index = HANDLE_TO_INDEX(hndl);
  if (index >= 0 && index < btif_max_avk_clients) {
    btif_a2dp_control_set_audio_delay(delay, index);
  } else {
    BTIF_TRACE_ERROR("%s: Invalid index for connection", __func__);
    btif_a2dp_control_set_audio_delay(delay, 0);
  }
}

void btif_avk_reset_audio_delay(tBTA_AVK_HNDL hndl) {
  int index = HANDLE_TO_INDEX(hndl);
  if (index >= 0 && index < btif_max_avk_clients) {
    btif_a2dp_control_reset_audio_delay(index);
  } else {
    BTIF_TRACE_ERROR("%s: Invalid index for connection", __func__);
    btif_a2dp_control_reset_audio_delay(0);
  }
}

uint16_t btif_avk_get_audio_delay(int index) {
  A2dpCodecConfig* current_codec = bta_avk_get_a2dp_current_codec();
  if(current_codec != nullptr) {
    if(current_codec->codecIndex() == BTAV_A2DP_CODEC_INDEX_SOURCE_APTX_ADAPTIVE) {
      BTIF_TRACE_WARNING("%s: Updating Aptx Adaptive specific delay: %d",
            __func__, btif_avk_cb[index].codec_latency);
      return btif_avk_cb[index].codec_latency;
    }
  }

  if (index >= 0 && index < btif_max_avk_clients) {
    return btif_a2dp_control_get_audio_delay(index);
  } else {
    BTIF_TRACE_ERROR("%s: Invalid index for connection", __func__);
    return btif_a2dp_control_get_audio_delay(0);
  }
}

uint16_t btif_avk_get_aptx_mode_info() {
  int index = btif_max_avk_clients;
  if (btif_avk_stream_started_ready())
    index = btif_avk_get_latest_playing_device_idx();
  else
    index = btif_avk_get_latest_device_idx_to_start();

  if(index >= btif_max_avk_clients) return 0;

  return btif_avk_cb[index].aptx_mode;
}

/*******************************************************************************
**
** Function         btif_avk_update_reported_delay
**
** Description      Count average packet delay (include buffering, decoding,
**                  rending delay)
**
** Returns          delay value (nanosencond)
*******************************************************************************/
uint64_t btif_avk_update_reported_delay(uint64_t inst_delay)
{
  average_delay = 0;
  if (delay_record_idx >= DELAY_RECORD_COUNT)
    delay_record_idx = 0;
  delay_record[delay_record_idx++] = inst_delay +
      ((get_audiotrack_latency() + MISC_RENDERING_DELAY) * 1000000);

  int64_t sum_delay = 0; int i = 0; int count = 0;
  for(; i < DELAY_RECORD_COUNT; i++) {
    if(delay_record[i] > 0) {
      sum_delay += delay_record[i];
      count++;
    }
    else
      break;
  }

  average_delay = (sum_delay / count);

  BTIF_TRACE_DEBUG("%s ~~inst_delay = [%09llu](ns) avg_delay = [%09llu](ns), Delay records = %d",
                    __func__,inst_delay, average_delay, count);

  return average_delay;
}

/*******************************************************************************
**
** Function         btif_avk_is_sink_delay_report_supported
**
** Description      check if the connected a2dp device supports
**                  delay reporting.
**
** Returns          bool
*******************************************************************************/
bool btif_avk_is_sink_delay_report_supported() {
  int index = 0;
  BTIF_TRACE_DEBUG("%s: %d" ,__func__, __func__, btif_avk_cb[index].avdt_sync);
  return btif_avk_cb[index].avdt_sync;
}

/*******************************************************************************
**
** Function         btif_avk_get_average_delay
**
** Description      Returns average of instantaneous delay values
**
** Returns          int64_t
*******************************************************************************/
int64_t btif_avk_get_average_delay() {
  return average_delay;
}

/*******************************************************************************
**
** Function         btif_avk_get_average_delay
**
** Description      To check if device is supporting which A2DP Role (Sink/Source)
**
** Returns          bool
*******************************************************************************/
bool btif_avk_device_in_sink_role() {
    char a2dp_role[6] = "false";
    osi_property_get("persist.vendor.service.bt.a2dp.sink", a2dp_role, "false");
    if (!strncmp("true", a2dp_role, 4))
        return true;
    return false;
}

/*******************************************************************************
**
** Function         btif_avk_is_a2dp_sink_handoff_required
**
** Description      To check if there is need for Soft-Handoff in A2DP Sink.
**
** Returns          bool
*******************************************************************************/
bool btif_avk_is_a2dp_sink_handoff_required(int idx) {
  int cur_idx = btif_avk_get_current_playing_dev_idx();
  BTIF_TRACE_DEBUG("%s: index = %d, current_index= %d" ,__func__, idx, cur_idx);
  if (idx != cur_idx) {
    return true;
  }
  return false;
}

/*******************************************************************************
**
** Function         btif_avk_initiate_sink_handoff
**
** Description      Intiates operations required to handle Soft-Handoff
**
** Returns          void
*******************************************************************************/
void btif_avk_initiate_sink_handoff(int idx, bool audio_state_changed) {
  int cur_idx = btif_avk_get_current_playing_dev_idx();
  BTIF_TRACE_DEBUG("%s: index = %d, Current_index= %d" ,__func__, idx, cur_idx);

  // Mark idx as current playing index
  btif_avk_cb[idx].current_playing = true;
  // Mark the other playing devices as not playing
  int i;
  for (i = 0; i < btif_max_avk_clients; i++) {
    if (i != idx) {
      btif_avk_cb[i].current_playing = false;
    }
  }
  other_device_media_packet_count = 0;
  // Update the decoder on soft-handoff if handoff is through audio state change
  if (audio_state_changed) {
      BTIF_TRACE_DEBUG("%s, updating decoder on SHO through audio state change", __func__);
      uint8_t* a2dp_codec_config = bta_avk_co_get_peer_codec_info(btif_avk_cb[idx].bta_handle);
      if (a2dp_codec_config != NULL) {
          /* Before create a new audiotrack, we need to stop and delete old audiotrack. */
          btif_a2dp_sink_audio_handle_stop_decoding();
          btif_a2dp_sink_clear_track_event();
          btif_a2dp_sink_update_decoder(a2dp_codec_config);
      } else {
          BTIF_TRACE_DEBUG("%s, a2dp_codec_config is NULL", __func__);
      }
  }
}

/*******************************************************************************
**
** Function         btif_avk_sink_ho_through_avrcp_pback_status
**
** Description      Initate SoftHandoff from AVRCP EVENT_PLAYBACK_STATUS_CHANGED
**                  (if recieved before AVDTP_START) Or in case of quick SHO
**                  i.e. prev streaming device started streaming again (within 3sec)
**                  and AVDTP_SUSPEND and AVDTP_START is not received but AVRCP cmd
**                  EVENT_PLAYBACK_STATUS_CHANGED is received.
**
** Returns          void
*******************************************************************************/
void btif_avk_sink_ho_through_avrcp_pback_status(RawAddress bd_addr) {
    int index = btif_avk_idx_by_bdaddr(&bd_addr);
    if (btif_avk_is_a2dp_sink_handoff_required(index)) {
        BTIF_TRACE_DEBUG("%s, handoff from avrcp playback status change event", __func__);
        btif_avk_initiate_sink_handoff(index, true);
    }
}

/*******************************************************************************
**
** Function         btif_avk_set_remote_playing_state
**
** Description      Sets remote device playing state when response is received after
**                  for passthrough command
**
** Returns          void
*******************************************************************************/
void btif_avk_set_remote_playing_state(int index, bool playing_state) {
  BTIF_TRACE_DEBUG("%s: is_remote_playing: %d", __func__, playing_state);
  if (index >= btif_max_avk_clients)
    return;
  btif_avk_cb[index].is_device_playing = playing_state;
}


/*******************************************************************************
**
** Function         btif_avk_get_max_allowable_sink_connections
**
** Description      Get maximum number of supported Sink Connections
**                  Currently, Default:1, Max:2
**                  TODO: Range:{1,5} Default:3 Max:5
**
** Returns          void
*******************************************************************************/
int btif_avk_get_max_allowable_sink_connections() {
    int def_no_of_conn = 1;
    char number_of_conn[2];
    osi_property_get("persist.vendor.bt.a2dp.sink_conn", number_of_conn, "1");
    BTIF_TRACE_DEBUG("%s, number_of_conn = %s", __func__, number_of_conn);
    if (!strncmp(number_of_conn, "2", 1)) {
        return atoi(number_of_conn);
    }
    int no_of_conn = atoi(number_of_conn);
    BTIF_TRACE_DEBUG("%s, number_of_conn set in property = %d", __func__, no_of_conn);
    if (no_of_conn < def_no_of_conn || no_of_conn > MAX_ALLOWED_SINK_CONNECTIONS) {
        return def_no_of_conn;
    }
    return no_of_conn;
}

/*******************************************************************************
**
** Function         btif_is_sink_delay_report_supported
**
** Description      check if the connected a2dp device supports
**                  delay reporting.
**
** Returns          bool
*******************************************************************************/
bool btif_is_sink_delay_report_supported() {
  int index = 0;
  BTIF_TRACE_DEBUG("%s: %d", __func__, btif_avk_cb[index].avdt_sync);
  return btif_avk_cb[index].avdt_sync;
}


