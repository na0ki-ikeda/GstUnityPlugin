LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := wrapper
LOCAL_SRC_FILES := wrapper.cpp debug.cpp
LOCAL_SHARED_LIBRARIES := gstreamer_android
LOCAL_LDLIBS := -llog -landroid -lGLESv1_CM -lGLESv2 -lGLESv3
include $(BUILD_SHARED_LIBRARY)

ifndef GSTREAMER_ROOT_ANDROID
$(error GSTREAMER_ROOT_ANDROID is not defined!)
endif

ifeq ($(TARGET_ARCH_ABI),armeabi)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm
ADDITIONAL_INC        := -I$(NDK_ROOT)/sysroot/usr/include/arm-linux-androideabi
TARGET_CFLAGS         := -target armv5te-none-linux-androideabi $(TARGET_CFLAGS)
TARGET_LDFLAGS        := -target armv5te-none-linux-androideabi $(TARGET_LDFLAGS)
else ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/armv7
ADDITIONAL_INC        := -I$(NDK_ROOT)/sysroot/usr/include/arm-linux-androideabi
TARGET_CFLAGS         := -target armv7-none-linux-androideabi $(TARGET_CFLAGS)
TARGET_LDFLAGS        := -target armv7-none-linux-androideabi $(TARGET_LDFLAGS)
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm64
ADDITIONAL_INC        := -I$(NDK_ROOT)/sysroot/usr/include/aarch64-linux-android
TARGET_CFLAGS         := -target aarch64-none-linux-android $(TARGET_CFLAGS)
TARGET_LDFLAGS        := -target aarch64-none-linux-android $(TARGET_LDFLAGS)
else ifeq ($(TARGET_ARCH_ABI),x86)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86
ADDITIONAL_INC        := -I$(NDK_ROOT)/sysroot/usr/include/i686-linux-android
TARGET_CFLAGS         := -target i686-none-linux-android $(TARGET_CFLAGS)
TARGET_LDFLAGS        := -target i686-none-linux-android $(TARGET_LDFLAGS)
else ifeq ($(TARGET_ARCH_ABI),x86_64)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86_64
ADDITIONAL_INC        := -I$(NDK_ROOT)/sysroot/usr/include/x86_64-linux-android
TARGET_CFLAGS         := -target x86_64-none-linux-android $(TARGET_CFLAGS)
TARGET_LDFLAGS        := -target x86_64-none-linux-android $(TARGET_LDFLAGS)
else
$(error Target arch ABI not supported: $(TARGET_ARCH_ABI))
endif

TARGET_CFLAGS             := $(TARGET_CFLAGS) -I$(NDK_ROOT)/sysroot/usr/include $(ADDITIONAL_INC)


GSTREAMER_NDK_BUILD_PATH  := $(GSTREAMER_ROOT)/share/gst-android/ndk-build/
include $(GSTREAMER_NDK_BUILD_PATH)/plugins.mk
GSTREAMER_PLUGINS         := $(GSTREAMER_PLUGINS_CORE) $(GSTREAMER_PLUGINS_PLAYBACK) $(GSTREAMER_PLUGINS_CODECS) $(GSTREAMER_PLUGINS_NET) $(GSTREAMER_PLUGINS_SYS) $(GSTREAMER_PLUGINS_EFFECTS) $(GSTREAMER_PLUGINS_CODECS_RESTRICTED)
GSTREAMER_EXTRA_DEPS      := gstreamer-video-1.0
include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer-1.0.mk
