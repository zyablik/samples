LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
include $(NGOS_VARS)

LOCAL_SRC_FILES:= main.cpp

LOCAL_MODULE := hello-grapevine

LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_SHARED_LIBRARIES := libgrapevine-vanda libvanda libgrapevine-foundation libseppo-common libseppo-protocol libsigcpp

LOCAL_CFLAGS            := -fexceptions -frtti -std=c++11
LOCAL_CXX_STL           := libc++

include $(BUILD_EXECUTABLE)
