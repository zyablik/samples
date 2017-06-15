LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := ion_hello_client.cpp
LOCAL_MODULE := ion-hello-client
LOCAL_C_INCLUDES := \
  system/core/include \

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := ion_hello_server.cpp
LOCAL_MODULE := ion-hello-server
LOCAL_C_INCLUDES := \
  system/core/include \

include $(BUILD_EXECUTABLE)
