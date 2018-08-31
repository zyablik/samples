LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := fb_hello
LOCAL_SRC_FILES := fb_hello.c
#LOCAL_SHARED_LIBRARIES := 
#LOCAL_CFLAGS := -DTARGET=ARM 
#LOCAL_C_INCLUDES := frameworks/native/include

include $(BUILD_EXECUTABLE)
