LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)          
LOCAL_MODULE    := demolish   
LOCAL_SRC_FILES := demolish.cpp
LOCAL_LDLIBS    := -llog
LOCAL_CFLAGS	:= -fpermissive -w
include $(BUILD_SHARED_LIBRARY) 