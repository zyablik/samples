LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
include $(NGOS_VARS)

LOCAL_SRC_FILES:= main.cpp

LOCAL_MODULE := gles-on-vanda-recreate

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    frameworks/appman/include \
    frameworks/graphics/libshm \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/ \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/build/17d76acd-target/base \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-00dev0/osu/platform \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/base/platform_dummy \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/osu/platform_dummy \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/stdlib/platform_dummy \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/kernel/drivers/gpu/arm \
    vendor/hisi/thirdparty/gpu/arm/bifrost-r2p0-02dev0/cl/src \


LOCAL_SHARED_LIBRARIES := libappman libGLES_mali_v2 libbinder libgrapevine-vanda libngshm libvanda libgrapevine-foundation libseppo-common libseppo-protocol libsigcpp libutils

LOCAL_CFLAGS            := -D__APPMAN__ -fexceptions -frtti -std=c++11 -Wno-narrowing -O0 -g3
#LOCAL_CXX_STL           := libc++

include $(BUILD_EXECUTABLE)
