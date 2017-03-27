LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := fb-native-window
LOCAL_CFLAGS += -std=c++11
LOCAL_SRC_FILES :=                \
    gralloc_stuff.cpp             \
    framebuffer_native_window.cpp \
    main.cpp                      \
    native_window_buffer.cpp      \

LOCAL_SHARED_LIBRARIES := libEGL libGLESv2 libui libcutils libbinder libgui libutils libhardware libsync

LOCAL_C_INCLUDES += \
    frameworks/native/include                   \
    system/core/libsync/include                 \
    vendor/hisi/ap/include/display/libgralloc   \
    vendor/hisi/ap/hardware/display/libgralloc
    
include $(BUILD_EXECUTABLE)
