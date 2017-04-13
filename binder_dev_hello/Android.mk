LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := binder-dev-hello-client
LOCAL_CFLAGS += -std=c++11

LOCAL_SRC_FILES := binder_dev_hello_client.cpp utils.cpp

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := binder-dev-hello-service
LOCAL_CFLAGS += -std=c++11

LOCAL_SRC_FILES := binder_dev_hello_service.cpp utils.cpp

include $(BUILD_EXECUTABLE)
