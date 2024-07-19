cmake_minimum_required(VERSION 3.0)

##set(CMAKE_CXX_COMPILER "/opt/rh/devtoolset-8/root/bin/g++")
##set(CMAKE_C_COMPILER "/opt/rh/devtoolset-8/root/bin/gcc")


project (tfs) 

#set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)
add_compile_options(-w)
# Debug or Release
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -rdynamic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -rdynamic")

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -rdynamic")
set(CMAKE_C_FLAGS_RELEASE  "${CMAKE_C_FLAGS_RELEASE} -rdynamic")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

message("CMAKE_BUILD_TYPE is:${CMAKE_BUILD_TYPE}")

# message("CMAKE_CXX_FLAGS_DEBUG :================")
# message(${CMAKE_CXX_FLAGS_DEBUG})

# message("CMAKE_CXX_FLAGS_RELEASE  :================")
# message(${CMAKE_CXX_FLAGS_RELEASE})

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR "CMAKE_BUILD_TYPE must be Debug or Release!!!")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    ADD_DEFINITIONS(-DNDEBUG)
    message("release")
endif()


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(-Wno-builtin-macro-redefined)

#add_definitions(-w)

#
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")

set(CXX_FLAGS
        -Wall
	-g
        )
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -save-temps")

SET(ROOT_DIR ${CMAKE_SOURCE_DIR})
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

execute_process(COMMAND sh ${ROOT_DIR}/make_depend.sh ${CMAKE_CURRENT_BINARY_DIR})

if(PRIMARYCHAIN)
    ADD_DEFINITIONS(-DPRIMARYCHAIN)
elseif(TESTCHAIN)
    ADD_DEFINITIONS(-DTESTCHAIN)
else()
    ADD_DEFINITIONS(-DDEVCHAIN)
endif()


file(GLOB SOURCES 
    "*.cpp"
    "api/*.cpp"
    "api/interface/*.cpp"
    "include/*.cpp" 
    "utils/*.cpp"
    "utils/json/*.cpp"
    "utils/*.c"
    "common/*.cpp"
    "ca/*.cpp"
    "ca/evm/*.cpp"
    "db/*.cpp"
    "net/*.cpp"
    "main/*.cpp"
    "proto/*.cc"
    "mpt/*.cpp"
    "contract/*.cpp"
    )

file(GLOB_RECURSE MAIN_FILE entry.cpp)
list(REMOVE_ITEM SOURCES ${MAIN_FILE})

# message("source:================")
# message(${SOURCES})
# message("MAIN_FILE:================")
# message(${MAIN_FILE})


#boost
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS regex system thread REQUIRED)


add_executable(${PROJECT_NAME}  ${MAIN_FILE} ${SOURCES})
include (utils.cmake)
redefine_file_macro(${PROJECT_NAME})


set(EXECUTABLE_OUTPUT_PATH bin)

include_directories("${ROOT_DIR}/")
include_directories("${ROOT_DIR}/ca")
include_directories("${ROOT_DIR}/ca/evm")
include_directories("${ROOT_DIR}/db")
include_directories("${ROOT_DIR}/include")
include_directories("${ROOT_DIR}/mpt")
include_directories("${ROOT_DIR}/rocksdb/include")
include_directories("${ROOT_DIR}/protobuf/src")
include_directories("${ROOT_DIR}/proto")
include_directories("${ROOT_DIR}/spdlog/include")
include_directories("${ROOT_DIR}/openssl/include")
include_directories("${ROOT_DIR}/evmone/evmc/include/")
include_directories("${ROOT_DIR}/evmone/include/")
include_directories("${ROOT_DIR}/wasmtime-cpp/include") 
include_directories("${ROOT_DIR}/silkpre/lib/")
include_directories("${ROOT_DIR}/silkpre/")
include_directories("${ROOT_DIR}/evmone/lib/")
include_directories("${ROOT_DIR}/boost")
include_directories("${ROOT_DIR}/contract")
#include_directories(${Boost_INCLUDE_DIRS})


target_link_libraries(${PROJECT_NAME}  dl)
target_link_libraries(${PROJECT_NAME}  z)
# target_link_libraries(${PROJECT_NAME}  bz2)
target_link_libraries(${PROJECT_NAME} -pthread ) 


target_link_libraries (${PROJECT_NAME} ${Boost_LIBRARIES})

add_library(rocksdb STATIC IMPORTED)
set_property(TARGET rocksdb PROPERTY IMPORTED_LOCATION ${ROOT_DIR}/rocksdb/librocksdb.a)
target_link_libraries(${PROJECT_NAME}  rocksdb )

add_library(cacore STATIC IMPORTED)
set_property(TARGET cacore PROPERTY IMPORTED_LOCATION ${ROOT_DIR}/ca_core/ca_core.a)
target_link_libraries(${PROJECT_NAME}  cacore )

add_library(protobuf STATIC IMPORTED)
#set_property(TARGET protobuf PROPERTY IMPORTED_LOCATION ${ROOT_DIR}/protobuf/cmake/libprotobuf.a)
set_property(TARGET protobuf PROPERTY IMPORTED_LOCATION ${ROOT_DIR}/protobuf/src/.libs/libprotobuf.a)
target_link_libraries(${PROJECT_NAME}  protobuf )

add_library(spdlog STATIC IMPORTED)
set_property(TARGET spdlog PROPERTY IMPORTED_LOCATION ${ROOT_DIR}/spdlog/libspdlog.a)
target_link_libraries(${PROJECT_NAME}  spdlog )

add_library(openssl STATIC IMPORTED)
set_property(TARGET openssl PROPERTY IMPORTED_LOCATION ${ROOT_DIR}/openssl/libssl.a)
set_property(TARGET openssl PROPERTY IMPORTED_LOCATION ${ROOT_DIR}/openssl/libcrypto.a)
target_link_libraries(${PROJECT_NAME}  openssl )

add_library(evmone STATIC IMPORTED)

set_property(TARGET evmone PROPERTY IMPORTED_LOCATION ${ROOT_DIR}/evmone/build/lib64/libevmone-standalone.a) 
target_link_libraries(${PROJECT_NAME}  evmone )

add_library(silkpre STATIC IMPORTED)
set_property(TARGET silkpre PROPERTY IMPORTED_LOCATION ${ROOT_DIR}/silkpre/build/lib/libsilkpre-standalone.a) 
target_link_libraries(${PROJECT_NAME}  silkpre )

add_library(wasmtime STATIC IMPORTED)
set_property(TARGET wasmtime PROPERTY IMPORTED_LOCATION ${ROOT_DIR}/wasmtime-cpp/lib/libwasmtime.a) 
target_link_libraries(${PROJECT_NAME} wasmtime)

add_library(cryptopp STATIC IMPORTED)
set_property(TARGET cryptopp PROPERTY IMPORTED_LOCATION ${ROOT_DIR}/cryptopp/libcryptopp.a)
target_link_libraries(${PROJECT_NAME}  cryptopp )

find_package(GTest)
if(GTEST_FOUND)
    # GTEST_INCLUDE_DIRS:/usr/local/include
    # message("GTEST_INCLUDE_DIRS:${GTEST_INCLUDE_DIRS}")
    include_directories(${GTEST_INCLUDE_DIRS})
    add_executable(test EXCLUDE_FROM_ALL ${SOURCES} ${TEST_SOURCE})
    
    # GTEST_BOTH_LIBRARIES:/usr/local/lib64/libgtest.a;/usr/local/lib64/libgtest_main.a
    message("GTEST_BOTH_LIBRARIES:${GTEST_BOTH_LIBRARIES}")
    # CMAKE_THREAD_LIBS_INIT:-lpthread, gtest
    message("CMAKE_THREAD_LIBS_INIT:${CMAKE_THREAD_LIBS_INIT}")

    target_link_libraries(test ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

    target_link_libraries(test  dl)
    target_link_libraries(test  z)
    target_link_libraries(test  bz2)
    target_link_libraries(test  pthread ) 
    target_link_libraries(test  rocksdb )
    target_link_libraries(test  protobuf )
    target_link_libraries(test  spdlog)
    target_link_libraries(test  openssl)

endif(GTEST_FOUND)

if(PRIMARYCHAIN)
    ADD_CUSTOM_COMMAND(TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND sh ${CMAKE_SOURCE_DIR}/gen_version_info.sh 0 ${CMAKE_CURRENT_BINARY_DIR}
    )
elseif(TESTCHAIN)
    ADD_CUSTOM_COMMAND(TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND sh ${CMAKE_SOURCE_DIR}/gen_version_info.sh 1 ${CMAKE_CURRENT_BINARY_DIR}
    )
else()
    ADD_CUSTOM_COMMAND(TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND sh ${CMAKE_SOURCE_DIR}/gen_version_info.sh 2 ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()

# install(CODE "execute_process(COMMAND sh ${CMAKE_SOURCE_DIR}/gen_version_info.sh)")
