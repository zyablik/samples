LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := fb-hello
LOCAL_CFLAGS += -std=c++11
LOCAL_SRC_FILES := fb_hello.cpp

include $(BUILD_EXECUTABLE)
