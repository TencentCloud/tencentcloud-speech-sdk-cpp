cmake_minimum_required(VERSION 2.8)
project(tencentcloud-speech-sdk-cpp)
set ( CMAKE_VERBOSE_MAKEFILE on )



set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -g ")

if(MSVC)
    set(CMAKE_CXX_FLAGS "/EHsc ${CMAKE_CXX_FLAGS}")
endif()

aux_source_directory(${CMAKE_SOURCE_DIR}/asr ASR_SRC_FILES)
aux_source_directory(${CMAKE_SOURCE_DIR}/common ASR_SRC_FILES)
include_directories(${CMAKE_SOURCE_DIR}/asr/include)
include_directories(${CMAKE_SOURCE_DIR}/common/include)

if(MSVC)
    include_directories(${CMAKE_SOURCE_DIR}/asr/include/pthread_win)
    set_property(TARGET ${TARGET_NAME} APPEND PROPERTY LINK_FLAGS "/NODEFAULTLIB")
endif()

include(CMakeHelpers.cmake)
init_target (asr_example)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
build_executable (${TARGET_NAME} ${ASR_SRC_FILES} examples/asr_example.cpp)
link_boost ()
final_target ()

if(MSVC)
if (CMAKE_CL_64)
	target_link_libraries(${TARGET_NAME} libboost_date_time-vc142-mt-gd-x64-1_74 libboost_regex-vc142-mt-gd-x64-1_74 libboost_random-vc142-mt-gd-x64-1_74 pthreadVC2 libcrypto libssl)
else()
	target_link_libraries(${TARGET_NAME} libboost_date_time-vc142-mt-gd-x32-1_74 libboost_regex-vc142-mt-gd-x32-1_74 libboost_random-vc142-mt-gd-x32-1_74 pthreadVSE2 libcrypto libssl)
endif ()
else ()
    target_link_libraries(${TARGET_NAME} -lboost_chrono -lboost_system -lboost_random -lpthread -lcrypto -lssl)
endif()







