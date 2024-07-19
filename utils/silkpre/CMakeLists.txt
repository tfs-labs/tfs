#[[
   Copyright 2022 The Silkpre Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
]]

# ethash requires cmake 3.16.2
cmake_minimum_required(VERSION 3.16.2)

option(SILKPRE_TESTING "Build tests and test tools" OFF)

get_directory_property(SILKPRE_HAS_PARENT PARENT_DIRECTORY)
if(NOT SILKPRE_HAS_PARENT)
    if(NOT CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain.cmake CACHE FILEPATH "" FORCE)
    endif()

    set(HUNTER_URL "https://github.com/cpp-pm/hunter/archive/v0.24.1.tar.gz")
    set(HUNTER_SHA1 "4942227a6e6f5e64414c55b97ef98609de199d18")

    if(SILKPRE_TESTING)
        set(HUNTER_PACKAGES benchmark Catch ethash intx)
    else()
        set(HUNTER_PACKAGES ethash intx)
    endif()

    include(FetchContent)
    FetchContent_Declare(SetupHunter GIT_REPOSITORY https://github.com/cpp-pm/gate)
    FetchContent_MakeAvailable(SetupHunter)
endif()

project(silkpre)

# TODO: disable exceptions


# GMP
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(GMP_INCLUDE_DIR "/usr/local/gmp/include")
set(GMP_LIBRARY "/usr/local/gmp/lib/libgmp.a")

if(NOT EXISTS ${GMP_INCLUDE_DIR})
  message(FATAL_ERROR "GMP include directory does not exist: ${GMP_INCLUDE_DIR}")
endif()

if(NOT EXISTS ${GMP_LIBRARY})
  message(FATAL_ERROR "GMP library file does not exist: ${GMP_LIBRARY}")
endif()

message(STATUS "Using manually set GMP library: ${GMP_LIBRARY}")

add_library(gmp STATIC IMPORTED)
set_target_properties(
    gmp PROPERTIES
    IMPORTED_LOCATION ${GMP_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${GMP_INCLUDE_DIR}
)

# secp256k1
add_library(secp256k1 third_party/secp256k1/src/secp256k1.c)
if(MSVC)
    target_link_libraries(secp256k1 PRIVATE gmp)
    target_compile_definitions(secp256k1 PUBLIC USE_NUM_GMP USE_FIELD_INV_NUM USE_SCALAR_INV_NUM)
    target_compile_definitions(secp256k1 PUBLIC USE_FIELD_10X26 USE_SCALAR_8X32)
    target_compile_options(secp256k1 PRIVATE /w) # Not much we can do about warnings
else()
    target_compile_definitions(secp256k1 PUBLIC USE_NUM_NONE USE_FIELD_INV_BUILTIN USE_SCALAR_INV_BUILTIN)
    target_compile_definitions(secp256k1 PUBLIC USE_FIELD_5X52 USE_SCALAR_4X64 HAVE___INT128)
endif()
target_compile_definitions(secp256k1 PUBLIC ECMULT_WINDOW_SIZE=15 ECMULT_GEN_PREC_BITS=4 USE_ENDOMORPHISM)
target_compile_definitions(secp256k1 PUBLIC ENABLE_MODULE_ECDH)
target_compile_definitions(secp256k1 PUBLIC ENABLE_MODULE_RECOVERY)
target_include_directories(secp256k1 PRIVATE secp256k1 INTERFACE third_party/secp256k1/include)

# libff
set(CURVE "ALT_BN128" CACHE STRING "" FORCE)
option(WITH_PROCPS "" OFF)
option(IS_LIBFF_PARENT "" OFF)
if(MSVC)
    option(MPIR_INSTEAD_OF_GMP "" ON)
endif()
add_subdirectory(third_party/libff)

add_subdirectory(lib)

if(SILKPRE_TESTING)
    add_subdirectory(test)
endif()
