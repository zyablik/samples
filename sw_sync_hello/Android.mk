LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := sw_sync_hello.cpp
LOCAL_MODULE := sw-sync-hello
#LOCAL_C_INCLUDES := system/core/include

include $(BUILD_EXECUTABLE)
