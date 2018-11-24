LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS += -std=c99

LOCAL_SRC_FILES := \
    mcucall.c \


LOCAL_MODULE:= mcucall
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
