LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE:= inject
LOCAL_SRC_FILES := inject.c shellcode.s
LOCAL_LDLIBS := -llog
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)          
LOCAL_MODULE    := injected  
LOCAL_SRC_FILES := load_dex.cpp
LOCAL_LDLIBS    := -llog -landroid_runtime
LOCAL_CFLAGS    := -I./include/
LOCAL_LDFLAGS	:=	-L./lib/ 
include $(BUILD_SHARED_LIBRARY) 