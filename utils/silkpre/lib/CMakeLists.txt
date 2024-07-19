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
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ethash CONFIG REQUIRED)
find_package(intx CONFIG REQUIRED)

function(add_standalone_library TARGET)
    get_target_property(type ${TARGET} TYPE)
    if(NOT type STREQUAL STATIC_LIBRARY)
        return()
    endif()

    set(name ${TARGET}-standalone)

    if(CMAKE_AR)
        # Generate ar linker script.
        set(script_file ${name}.mri)
        set(script "CREATE $<TARGET_FILE:${name}>\n")
        string(APPEND script "ADDLIB $<TARGET_FILE:${TARGET}>\n")

        get_target_property(link_libraries ${TARGET} LINK_LIBRARIES)
        foreach(lib ${link_libraries})
            get_target_property(type ${lib} TYPE)
            if(NOT type STREQUAL INTERFACE_LIBRARY)
                string(APPEND script "ADDLIB $<TARGET_FILE:${lib}>\n")
            endif()
        endforeach()

        string(APPEND script "SAVE\n")
        file(GENERATE OUTPUT ${script_file} CONTENT ${script})

        # Add -standalone static library.
        add_library(${name} STATIC)
        target_sources(${name} PRIVATE ${script_file})
        add_custom_command(TARGET ${name} POST_BUILD COMMAND ${CMAKE_AR} -M < ${script_file})
        add_dependencies(${name} ${TARGET})

        get_property(enabled_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
        list(GET enabled_languages -1 lang)
        set_target_properties(${name} PROPERTIES LINKER_LANGUAGE ${lang})
    endif()
endfunction()

add_library(silkpre
    silkpre/blake2b.c
    silkpre/blake2b.h
    silkpre/ecdsa.c
    silkpre/ecdsa.h
    silkpre/precompile.cpp
    silkpre/precompile.h
    silkpre/rmd160.c
    silkpre/rmd160.h
    silkpre/secp256k1n.cpp
    silkpre/secp256k1n.hpp
    silkpre/sha256.c
    silkpre/sha256.h
)
target_include_directories(silkpre PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(silkpre PUBLIC intx::intx secp256k1 PRIVATE ethash::keccak ff gmp)

add_standalone_library(silkpre)
