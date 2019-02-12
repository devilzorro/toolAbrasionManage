cmake_minimum_required(VERSION 3.9)
project(toolAbrasionManage)

ADD_DEFINITIONS(-DJSON_IS_AMALGAMATION)

if (UNIX)
unset(source_files)

set(CMAKE_CXX_STANDARD 11)

set(srcdirs
        src/json/
        src/redisClient/
        .)

include_directories(
        includes/
        src/
)

foreach(srcdir ${srcdirs})
    aux_source_directory(${srcdir} source_files)
endforeach()

link_directories(
        libs/hiredis/
        libs/mqtt/
        .
)
add_executable(toolAbrasionManage ${source_files})
message("on unix platfrom")
target_link_libraries(toolAbrasionManage hiredis paho-mqttpp3 pthread)

elseif(WIN32)

else()
    message("on a unsupport platfrom")
endif ()




