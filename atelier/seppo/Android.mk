LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
include $(NGOS_VARS)

LOCAL_MODULE := seppo-server
LOCAL_SRC_FILES := server.cpp
LOCAL_SHARED_LIBRARIES := libseppo-common libseppo-protocol libsigcpp

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
include $(NGOS_VARS)

LOCAL_MODULE := seppo-client
LOCAL_SRC_FILES := client.cpp
LOCAL_SHARED_LIBRARIES := libseppo-common libseppo-protocol libsigcpp

include $(BUILD_EXECUTABLE)
