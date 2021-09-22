ACLOCAL_AMFLAGS = -I m4

#exporting stack headers
stack_includedir = $(includedir)/
stack_include_HEADERS = $(top_srcdir)/system_bt_ext/stack/include/a2dp_vendor_aptx_tws.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/stack/include/a2dp_vendor_aptx_tws_constants.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/stack/include/a2dp_vendor_aptx_tws_encoder.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/stack/include/a2d_aptx_ad_decoder.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/stack/btm/btm_csb.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/stack/include/a2d_sbc.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/stack/include/a2d_aac.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/stack/include/a2d_mp3.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/stack/include/a2d_aptx.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/stack/include/a2d_aptx_hd.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/btif/include/btif_tws_plus.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/btif/include/btif_avk_co.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/btif/include/btif_avk.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/btif/include/btif_avk_media.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/btif/include/btif_avk_api.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/udrv/include/uipc_avk.h
stack_include_HEADERS += $(top_srcdir)/system_bt_ext/btif/include/btif_bat.h

#exporting system_bt_ext headers
systemBtExt_includedir = $(includedir)/
systemBtExt_include_HEADERS = $(top_srcdir)/system_bt_ext/device/include/interop_config.h
systemBtExt_include_HEADERS += $(top_srcdir)/system_bt_ext/device/include/profile_config.h
systemBtExt_include_HEADERS += $(top_srcdir)/system_bt_ext/bta/ar/bta_ar_int_ext.h
systemBtExt_include_HEADERS += $(top_srcdir)/system_bt_ext/bta/include/bta_avk_api.h
systemBtExt_include_HEADERS += $(top_srcdir)/system_bt_ext/bta/include/bta_avk_co.h
systemBtExt_include_HEADERS += $(top_srcdir)/system_bt_ext/bta/include/bta_avk_aac.h
systemBtExt_include_HEADERS += $(top_srcdir)/system_bt_ext/bta/include/bta_avk_mp3.h
systemBtExt_include_HEADERS += $(top_srcdir)/system_bt_ext/bta/include/bta_avk_ci.h
systemBtExt_include_HEADERS += $(top_srcdir)/system_bt_ext/bta/include/bta_avk_sbc.h
systemBtExt_include_HEADERS += $(top_srcdir)/system_bt_ext/bta/include/bta_tws_plus_api.h
systemBtExt_include_HEADERS += $(top_srcdir)/system_bt_ext/bta/include/bta_ag_twsp_dev.h
systemBtExt_include_HEADERS += $(top_srcdir)/system_bt_ext/bta/include/bta_ag_twsp.h
systemBtExt_include_HEADERS += $(top_srcdir)/system_bt_ext/bta/include/bta_bat.h
systemBtExt_include_HEADERS += $(top_srcdir)/system_bt_ext/include/bt_testapp.h


#exporting interop header
interop_includedir = $(includedir)/device/include/
interop_include_HEADERS =  $(top_srcdir)/system_bt_ext/device/include/interop_config.h
interop_include_HEADERS +=  $(top_srcdir)/system_bt_ext/device/include/profile_config.h

#exporting btif headers
btif_includedir = $(includedir)/btif/include/
btif_include_HEADERS = $(top_srcdir)/system_bt_ext/btif/include/btif_avk.h


#exporting vhalhardware headers
vhalhardware_includedir = $(includedir)/hardware/
vhalhardware_include_HEADERS = $(top_srcdir)/vhal/include/hardware/vendor.h
vhalhardware_include_HEADERS += $(top_srcdir)/vhal/include/hardware/bt_av_vendor.h
vhalhardware_include_HEADERS += $(top_srcdir)/vhal/include/hardware/bt_hf_vendor.h
vhalhardware_include_HEADERS += $(top_srcdir)/vhal/include/hardware/bt_rc_vendor.h
vhalhardware_include_HEADERS += $(top_srcdir)/vhal/include/hardware/bt_hh_vendor.h
vhalhardware_include_HEADERS += $(top_srcdir)/vhal/include/hardware/vendor_socket.h
vhalhardware_include_HEADERS += $(top_srcdir)/vhal/include/hardware/bt_vendor_rc.h
vhalhardware_include_HEADERS += $(top_srcdir)/vhal/include/hardware/bt_ba.h
vhalhardware_include_HEADERS += $(top_srcdir)/vhal/include/hardware/bt_hf_client_vendor.h

vhal_includedir = $(includedir)/
vhal_include_HEADERS = $(top_srcdir)/vhal/include/bt_vendor_av.h

pkgconfigdir   = $(libdir)/pkgconfig
pkgconfig_DATA = bluetoothExt.pc
EXTRA_DIST     = $(pkgconfig_DATA)
