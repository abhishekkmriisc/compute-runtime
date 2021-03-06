# Copyright (c) 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

set(IGDRCL_SRCS_tests_gen8
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_gen8.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/coherency_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/enqueue_media_kernel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gen_cmd_parse.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/image_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/scheduler_dispatch_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_device_queue_hw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_preamble.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_preemption.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_sample.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/device_tests_bwd.cpp"
    PARENT_SCOPE
)

if(NOT MSVC)
  set(IGDRCL_SRCS_tests_gen8_linux
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/hw_info_config_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_mapper_tests.cpp"
    PARENT_SCOPE
  )
  
  list(APPEND IGDRCL_SRCS_linux_dll_tests_gen8
      ${CMAKE_CURRENT_SOURCE_DIR}/linux/dll/device_id_tests.cpp
  )
endif(NOT MSVC)

if(MSVC)
  set(IGDRCL_SRCS_tests_gen8_windows
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/wddm_mapper_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/translationtable_callbacks_tests.cpp"
    PARENT_SCOPE
  )
else()
  set(IGDRCL_SRCS_linux_dll_tests_gen8
    ${IGDRCL_SRCS_linux_dll_tests_gen8}
    PARENT_SCOPE
  )
endif(MSVC)
