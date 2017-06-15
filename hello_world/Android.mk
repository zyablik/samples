LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := hello-world
LOCAL_CFLAGS += -std=c++11
LOCAL_SRC_FILES := hello.cpp

include $(BUILD_EXECUTABLE)
