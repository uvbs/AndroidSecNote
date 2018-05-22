LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE:= testhook
LOCAL_SRC_FILES := testhook.cpp
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)          
LOCAL_MODULE    := gothook  
LOCAL_SRC_FILES := gothook.cpp
include $(BUILD_SHARED_LIBRARY) 