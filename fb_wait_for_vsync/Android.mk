LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := fb-wait4vsync
LOCAL_CFLAGS += -std=c++11
LOCAL_SRC_FILES := fb_wait_for_vsync.cpp

include $(BUILD_EXECUTABLE)
