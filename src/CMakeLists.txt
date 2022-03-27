cmake_minimum_required(VERSION 3.16.3)
project(grid_2d C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_COMPILER_NAMES clang)
# set( CMAKE_VERBOSE_MAKEFILE on )

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(isDebug TRUE)
else()
    set(isDebug FALSE)
endif()

message("isDebug: ${isDebug}")

set(useSanitizers ${isDebug})
message("useSanitizers: ${useSanitizers}")

if (useSanitizers)
    message("using sanitizers")
    set(sanitizers "-fsanitize=address")
endif()

if (APPLE)
        add_compile_definitions(TORNADO_OS_MACOS)
        set(OS_MACOS 1)
elseif (UNIX)
        add_compile_definitions(TORNADO_OS_LINUX)
        set(OS_LINUX 1)
elseif (WIN32)
        add_compile_definitions(TORNADO_OS_WINDOWS)
        set(OS_WINDOWS 1)

endif()

if (isDebug)
        message("Debug build detected")
        set(CONFIGURATION_DEBUG 1)
endif()

add_compile_definitions(_POSIX_C_SOURCE=200112L)


include_directories(include)

file(GLOB_RECURSE lib_src FOLLOW_SYMLINKS
        "lib/*.c"
        )

function(unixlike)
        message("unixlike")
endfunction()

if (OS_LINUX)
        message("Linux Detected!")
        unixlike()

elseif (OS_MACOS)
        message("MacOS detected!")
        unixlike()
endif()


add_library(grid_2d
        ${lib_src}
)

set (deps ../deps/)

target_include_directories(grid_2d PUBLIC ${deps}piot/basal-c/src/include)
target_include_directories(grid_2d PUBLIC ${deps}piot/imprint/src/include)
target_include_directories(grid_2d PUBLIC ${deps}piot/clog/src/include)
target_include_directories(grid_2d PUBLIC ${deps}piot/tiny-libc/src/include)

set_target_properties(grid_2d
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if (useSanitizers)
    target_link_libraries(grid_2d ${sanitizers})
endif()


target_compile_options(grid_2d PRIVATE -Wall -Wpedantic -Wextra ${sanitizers})

if (NOT isDebug)
    message("optimize!")
    target_compile_options(grid_2d PRIVATE -O3)
endif()

target_link_libraries(grid_2d m)
