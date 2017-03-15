LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := binder-hello
LOCAL_CFLAGS += -std=c++11
LOCAL_SHARED_LIBRARIES += libutils libbinder

LOCAL_SRC_FILES := binder_hello.cpp

include $(BUILD_EXECUTABLE)
