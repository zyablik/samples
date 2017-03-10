LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
include $(NGOS_VARS)

LOCAL_MODULE := vanda-client
LOCAL_SRC_FILES := client.cpp
LOCAL_SHARED_LIBRARIES := libvanda libseppo-common libseppo-protocol libsigcpp
LOCAL_CFLAGS += -O0 -g3

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
include $(NGOS_VARS)

LOCAL_MODULE := vanda-client-application
LOCAL_SRC_FILES := client_application.cpp
LOCAL_SHARED_LIBRARIES := libvanda libseppo-common libseppo-protocol libsigcpp
LOCAL_CFLAGS += -O0 -g3

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
include $(NGOS_VARS)

LOCAL_MODULE := vanda-compositor
LOCAL_SRC_FILES := compositor.cpp
LOCAL_SHARED_LIBRARIES := libvanda libseppo-common libseppo-protocol libsigcpp

include $(BUILD_EXECUTABLE)
