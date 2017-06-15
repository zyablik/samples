LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := inotify_test
LOCAL_SRC_FILES := main.cpp
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_C_INCLUDES := system/core/include

include $(BUILD_EXECUTABLE)

