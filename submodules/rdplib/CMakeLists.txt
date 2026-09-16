cmake_minimum_required(VERSION 3.16)

project(rdplib C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(RDPLIB_BUILD_SHARED "Build the shared rdplib library" ON)
option(RDPLIB_SOURCE_FAITHFUL "Select recovered source behavior instead of the default bounded behavior" OFF)

find_package(Threads REQUIRED)

set(RDPLIB_SOURCES
    src/bandwidth.c
    src/bitops.c
    src/connection.c
    src/connhash.c
    src/cmp.c
    src/crc.c
    src/cypher.c
    src/dpc.c
    src/dpf.c
    src/eventq.c
    src/fast_malloc.c
    src/hash.c
    src/list.c
    src/log.c
    src/msg_arrival.c
    src/msg_outgoing.c
    src/rdplib.c
    src/rdplib_rdpstat.c
    src/pqueue.c
    src/txq.c
    src/rdp.c
    src/rx.c
    src/rxq.c
    src/serial.c
    src/serial_rx.c
    src/rdpstat.c
    src/timeout.c
    src/trace.c
    src/tx.c
    src/usend.c
    src/ustrerror.c
)

if(WIN32)
    list(APPEND RDPLIB_SOURCES
        platform/windows/rdplib_platform_host.c
        platform/windows/uevent.c
        platform/windows/uioctl.c
        platform/windows/usemaphore.c
        platform/windows/uthread.c
        platform/windows/utime.c
    )
    set(RDPLIB_PLATFORM_INCLUDE platform/windows)
elseif(UNIX)
    list(APPEND RDPLIB_SOURCES
        platform/posix/rdplib_platform_host.c
        platform/posix/uevent.c
        platform/posix/uioctl.c
        platform/posix/usemaphore.c
        platform/posix/uthread.c
        platform/posix/utime.c
    )
    set(RDPLIB_PLATFORM_INCLUDE platform/posix)
else()
    message(FATAL_ERROR "rdplib currently supports Windows and POSIX hosts")
endif()

if(NOT RDPLIB_SOURCE_FAITHFUL)
    list(APPEND RDPLIB_SOURCES src/rdplib_random.c)
endif()

set_source_files_properties(
    src/connection.c
    src/rdp.c
    src/rx.c
    src/tx.c
    PROPERTIES COMPILE_DEFINITIONS RDPLIB_DISCARD_GLOBAL_STATISTICS
)

function(rdplib_configure_library target_name)
    target_include_directories(${target_name}
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/${RDPLIB_PLATFORM_INCLUDE}
    )

    if(RDPLIB_SOURCE_FAITHFUL)
        target_compile_definitions(${target_name} PRIVATE RDPLIB_SOURCE_FAITHFUL)
    endif()
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
        target_link_libraries(${target_name} PUBLIC ws2_32 winmm)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            # Recovered headers intentionally emit ordinary static helpers into
            # translation units that do not necessarily call every helper.
            target_compile_options(${target_name} PRIVATE -Wno-unused-function)
        endif()
        target_link_libraries(${target_name} PUBLIC Threads::Threads)
        if(UNIX)
            target_compile_definitions(${target_name} PRIVATE _POSIX_C_SOURCE=200809L)
            if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT RDPLIB_SOURCE_FAITHFUL)
                target_compile_definitions(${target_name} PRIVATE _GNU_SOURCE)
            endif()
            target_link_libraries(${target_name} PUBLIC m)
        endif()
    endif()
endfunction()

add_library(rdplib STATIC ${RDPLIB_SOURCES})
rdplib_configure_library(rdplib)
if(RDPLIB_SOURCE_FAITHFUL)
    # The static target also supports the raw interface. Its consumers need
    # the same profile because declarations and layouts change.
    target_compile_definitions(rdplib INTERFACE RDPLIB_SOURCE_FAITHFUL)
endif()

if(RDPLIB_BUILD_SHARED)
    add_library(rdplib_shared SHARED ${RDPLIB_SOURCES})
    rdplib_configure_library(rdplib_shared)
    target_compile_definitions(rdplib_shared PRIVATE RDPLIB_SHARED_BUILD INTERFACE RDPLIB_SHARED)
    set_target_properties(rdplib_shared PROPERTIES OUTPUT_NAME rdplib POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)

    if(WIN32)
        set_target_properties(rdplib_shared PROPERTIES ARCHIVE_OUTPUT_NAME rdplib_import PDB_NAME rdplib_dll)
    endif()
endif()
