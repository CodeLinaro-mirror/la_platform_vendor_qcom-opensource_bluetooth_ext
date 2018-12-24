AM_CPPFLAGS = -O2 \
            -D_GNU_SOURCE \
            -DHAS_NO_BDROID_BUILDCFG \
            -std=c++11

AM_CFLAGS = -Wall \
            -Wundef \
            -Wstrict-prototypes \
            -Wno-trigraphs \
            -std=c++11 \
            -DHAS_NO_BDROID_BUILDCFG


ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS += -I${WORKSPACE}/vendor/qcom/opensource/system/bt/stack/include \
    -I${WORKSPACE}/vendor/qcom/opensource/system/bt/include \
    -I${WORKSPACE}/vendor/qcom/opensource/system/bt/internal_include \
    -I${WORKSPACE}/vendor/qcom/opensource/system/bt/stack/l2cap \
    -I${WORKSPACE}/vendor/qcom/opensource/system/bt/utils/include \
    -I${WORKSPACE}/vendor/qcom/opensource/system/bt \
    -I${WORKSPACE}/vendor/qcom/opensource/system/bt/btif/include \
    -I${WORKSPACE}/vendor/qcom/opensource/system/bt/bta/include \
    -I${WORKSPACE}/vendor/qcom/opensource/bluetooth_ext/system_bt_ext/include \
    -I${WORKSPACE}/vendor/qcom/opensource/bluetooth_ext/vhal/include \
    -I${WORKSPACE}/vendor/qcom/opensource/system/bt/types \
    -I${WORKSPACE}/system/core/include \
    -I$(bdroid_C_INCLUDES)



cpp_sources = l2test_ertm.cpp


if USE_GST
requiredlibs = -lpthread -lm -ldl -lrt -lbtvendorhal -lbttypes -lchrome
else
requiredlibs = -lpthread -lm -ldl -lrt -lbtvendorhal -lbttypes -lchrome
endif


if USE_GLIB

if USE_GST
 gstl2test_ertm_CFLAGS  = $(AM_CFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
 gstl2test_ertm_CPPFLAGS  = $(AM_CPPFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
 gstl2test_ertm_LDFLAGS = $(requiredlibs) @GLIB_LIBS@
 gstl2test_ertm_CPPFLAGS += $(GSTREAMER_CFLAGS) -DUSE_GST
else
 l2test_ertm_CFLAGS  = $(AM_CFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
 l2test_ertm_CPPFLAGS  = $(AM_CPPFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
 l2test_ertm_LDFLAGS = $(requiredlibs) @GLIB_LIBS@
endif

else

if USE_GST
 gstl2test_ertm_CFLAGS = $(AM_CFLAGS)
 gstl2test_ertm_LDFLAGS = $(requiredlibs)
else
 l2test_ertm_CFLAGS = $(AM_CFLAGS)
 l2test_ertm_LDFLAGS = $(requiredlibs)
endif

endif

if USE_GST
gstl2test_ertm_CFLAGS += $(GSTREAMER_CFLAGS)
endif

AM_LDFLAGS = $(LDFLAFGS)

if USE_GST
bin_PROGRAMS = gstl2test_ertm
gstl2test_ertm_CC = @CC@
gstl2test_ertm_SOURCES = $(cpp_sources)
else
bin_PROGRAMS = l2test_ertm
l2test_ertm_CC = @CC@
l2test_ertm_SOURCES = $(cpp_sources)
endif
pkgconfigdir = $(libdir)/pkgconfig
