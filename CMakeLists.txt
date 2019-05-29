cmake_minimum_required(VERSION 3.9)
project(toolLife)

ADD_DEFINITIONS(-DJSON_IS_AMALGAMATION)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_BUILD_TYPE "release")

if (UNIX)
unset(source_files)

SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -fpermissive")


set(srcdirs
        src/json/
        src/redisClient/
        src/config/
        src/Log4C/
        src/fileCtl/
        src/unzip/
        .)

include_directories(
        includes/
        src/
)

foreach(srcdir ${srcdirs})
    aux_source_directory(${srcdir} source_files)
endforeach()

#link_directories(
#        libs/hiredis/
#        libs/mqtt/
#        libs/tooldll/
#        .
#)
    find_library(
            libs/hiredis/
            libs/log4c/
            libs/mqtt/
            libs/miniz/
    )

    link_directories(
            #/home/i5/bin/toolLife/
            ${CMAKE_SOURCE_DIR}/libs/hiredis/
            ${CMAKE_SOURCE_DIR}/libs/log4c/
            ${CMAKE_SOURCE_DIR}/libs/mqtt/
            ${CMAKE_SOURCE_DIR}/libs/miniz/
    )

message("on unix platfrom")
add_executable(toolLife ${source_files})
target_link_libraries(toolLife libhiredis.a libpaho-mqttpp3.so libminiz.a log4cplus pthread dl uuid)
set_target_properties(toolLife PROPERTIES INSTALL_RPATH "./")
install(TARGETS toolLife DESTINATION ${CMAKE_SOURCE_DIR}/target)

elseif(WIN32)
    message("on windows platfrom")
    unset(source_files)

    set(srcdirs
            src/json/
            src/redisClient/
            src/Win32_Interop/
            src/config/
            src/Log4C/
            src/unzip-win/
            src/fileCtl/
            #src/win_stdafx/
            .)

    include_directories(
            includes/
            #includes/hiredis-win/
            #includes/mqtt-win/
            src/
    )

    foreach(srcdir ${srcdirs})
        aux_source_directory(${srcdir} source_files)
    endforeach()

    link_directories(
            libs/hiredis-win/
            libs/mqtt-win/
            libs/tooldll-win/
            libs/log4c-win/
            libs/miniz-win/
            .
    )
    add_executable(toolLife ${source_files})
    target_link_libraries(toolLife hiredis paho-mqtt3a paho-mqttpp3 log4cplus miniz)

else()
    message("on a unsupport platfrom")
endif ()






