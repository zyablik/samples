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
LOCAL_SHARED_LIBRARIES := libEGL libGLESv2 libui libcutils libbinder libgui libutils libhardware libsync
LOCAL_C_INCLUDES :=  \
    frameworks/native/include   \
    system/core/libsync/include \


include $(BUILD_EXECUTABLE)
