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

###
### wificond daemon.
###
include $(CLEAR_VARS)
LOCAL_MODULE := wificond
LOCAL_CLANG := true
LOCAL_CPPFLAGS := -std=c++11 -Wall -Werror -Wno-unused-parameter
LOCAL_INIT_RC := wificond.rc
LOCAL_SRC_FILES := \
    main.cpp
LOCAL_SHARED_LIBRARIES := \
    libbase
include $(BUILD_EXECUTABLE)

###
### wificond unit tests.
###
include $(CLEAR_VARS)
LOCAL_MODULE := wificond_unit_test
LOCAL_CPPFLAGS := -std=c++11 -Wall -Werror -Wno-unused-parameter
LOCAL_SRC_FILES := \
    tests/main.cpp \
    tests/wificond_unittest.cpp
LOCAL_STATIC_LIBRARIES := \
    libgmock_host
include $(BUILD_HOST_NATIVE_TEST)
