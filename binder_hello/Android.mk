LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := binder-hello
LOCAL_CFLAGS += -std=c++11 -g3 -O0
LOCAL_SHARED_LIBRARIES += libutils libbinder libngshm 

LOCAL_SRC_FILES := binder_hello.cpp

LOCAL_C_INCLUDES := \
    frameworks/graphics/libshm

include $(BUILD_EXECUTABLE)
