LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := unix-sock-client
LOCAL_CFLAGS += -std=c++11

LOCAL_SRC_FILES := unix_sock_hello_client.cpp

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := unix-sock-server
LOCAL_CFLAGS += -std=c++11

LOCAL_SRC_FILES := unix_sock_hello_server.cpp

include $(BUILD_EXECUTABLE)
