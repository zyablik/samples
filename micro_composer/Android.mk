LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := micro-composer
LOCAL_SRC_FILES :=           \
    hwc_native_window.cpp    \
    micro_composer.cpp       \
    native_window_buffer.cpp \
    native_window_stub.cpp   \
    utils.cpp                \

LOCAL_CFLAGS := -O0 -g3
LOCAL_SHARED_LIBRARIES := libEGL libGLESv2 libui libcutils libbinder libgui libutils libhardware libsync
LOCAL_C_INCLUDES :=  \
    frameworks/native/include   \
    system/core/libsync/include \

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := micro-client
LOCAL_SRC_FILES :=          \
    hwc_native_window.cpp    \
    micro_client.cpp        \
    native_window_buffer.cpp \
    native_window_proxy.cpp \
    utils.cpp               \

LOCAL_CFLAGS := -O0 -g3
LOCAL_SHARED_LIBRARIES := libEGL libGLESv2 libGLES_mali_v2 libui libcutils libbinder libgui libutils libhardware libsync
LOCAL_C_INCLUDES :=  \
    frameworks/native/include   \
    system/core/libsync/include \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/ \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/build/17d76acd-target/base \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-00dev0/osu/platform \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/base/platform_dummy \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/osu/platform_dummy \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/stdlib/platform_dummy \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/kernel/drivers/gpu/arm \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/cl/src \


include $(BUILD_EXECUTABLE)
