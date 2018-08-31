LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := set-affinity
LOCAL_CFLAGS += -std=c++11

LOCAL_SRC_FILES := set_affinity.cpp

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := power-test
LOCAL_CFLAGS += -std=c++11

LOCAL_SRC_FILES := power_test.cpp

include $(BUILD_EXECUTABLE)
