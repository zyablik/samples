LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
include $(NGOS_VARS)

LOCAL_MODULE := ng-shm-client
LOCAL_SRC_FILES := ng_shm_client.cpp
LOCAL_SHARED_LIBRARIES := libngshm libbinder libui
LOCAL_C_INCLUDES += \
    frameworks/graphics/libshm \
    frameworks/native/include

LOCAL_CFLAGS += -O0 -g3

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
include $(NGOS_VARS)

LOCAL_MODULE := ng-shm-service
LOCAL_SRC_FILES := ng_shm_service.cpp
LOCAL_SHARED_LIBRARIES := libngshm libbinder libui
LOCAL_CFLAGS += -O0 -g3

include $(BUILD_EXECUTABLE)
