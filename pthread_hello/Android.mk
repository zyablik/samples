LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := pthread-hello
LOCAL_CFLAGS += -std=c++11

LOCAL_SRC_FILES := pthread_hello.cpp

include $(BUILD_EXECUTABLE)
