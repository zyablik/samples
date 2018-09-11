LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := gles_fbo_cube
LOCAL_SRC_FILES := fbo_cube.cpp runtime.cpp matrix.cpp shaders.cpp timer.cpp
LOCAL_SHARED_LIBRARIES := libEGL libGLESv2 libui libcutils libbinder libgui libutils
LOCAL_CFLAGS := -DTARGET=ARM 
LOCAL_C_INCLUDES := frameworks/native/include

include $(BUILD_EXECUTABLE)
