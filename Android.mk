# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

wificond_cpp_flags := -std=c++11 -Wall -Werror -Wno-unused-parameter
wificond_cpp_src := looper_backed_event_loop.cpp

wificond_test_util_cpp_src := \
    tests/shell_utils.cpp \

###
### wificond daemon.
###
include $(CLEAR_VARS)
LOCAL_MODULE := wificond
LOCAL_CPPFLAGS := $(wificond_cpp_flags)
LOCAL_INIT_RC := wificond.rc
LOCAL_AIDL_INCLUDES += $(LOCAL_PATH)/aidl
LOCAL_SRC_FILES := \
    main.cpp \
    aidl/android/wificond/IServer.aidl \
    server.cpp
LOCAL_SHARED_LIBRARIES := \
    libbinder \
    libbase \
    libutils
LOCAL_STATIC_LIBRARIES := \
    libwificond
include $(BUILD_EXECUTABLE)

###
### wificond static library
###
include $(CLEAR_VARS)
LOCAL_MODULE := libwificond
LOCAL_CPPFLAGS := $(wificond_cpp_flags)
LOCAL_SRC_FILES := $(wificond_cpp_src)
LOCAL_SHARED_LIBRARIES := \
    libbase \
    libutils
include $(BUILD_STATIC_LIBRARY)

###
### host version of the wificond static library (for use in tests)
###
include $(CLEAR_VARS)
LOCAL_MODULE := libwificond_host
LOCAL_CPPFLAGS := $(wificond_cpp_flags)
LOCAL_SRC_FILES := $(wificond_cpp_src)
LOCAL_STATIC_LIBRARIES := \
    libbase \
    libutils
LOCAL_MODULE_HOST_OS := linux
include $(BUILD_HOST_STATIC_LIBRARY)

###
### host version of test util library
###
include $(CLEAR_VARS)
LOCAL_MODULE := libwificond_test_utils_host
LOCAL_MODULE_HOST_OS := linux
LOCAL_CPPFLAGS := $(wificond_cpp_flags)
LOCAL_LDLIBS_linux := -lrt
LOCAL_SRC_FILES := $(wificond_test_util_cpp_src)
LOCAL_STATIC_LIBRARIES := \
    libbase
include $(BUILD_HOST_STATIC_LIBRARY)

###
### wificond host unit tests.
###
include $(CLEAR_VARS)
LOCAL_MODULE := wificond_unit_test
LOCAL_CPPFLAGS := $(wificond_cpp_flags)
LOCAL_LDLIBS_linux := -lrt
LOCAL_SRC_FILES := \
    tests/main.cpp \
    tests/looper_backed_event_loop_unittest.cpp \
    tests/shell_unittest.cpp
LOCAL_STATIC_LIBRARIES := \
    libgmock_host \
    libwificond_host \
    libwificond_test_utils_host \
    libbase \
    libutils \
    liblog
LOCAL_MODULE_HOST_OS := linux
include $(BUILD_HOST_NATIVE_TEST)
