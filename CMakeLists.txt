cmake_minimum_required(VERSION 3.9)
project(toolAbrasionManage)

ADD_DEFINITIONS(-DJSON_IS_AMALGAMATION)

set(CMAKE_CXX_STANDARD 11)

if (UNIX)
unset(source_files)

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
message("on unix platfrom")
add_executable(toolAbrasionManage ${source_files})
target_link_libraries(toolAbrasionManage hiredis paho-mqttpp3 pthread)

elseif(WIN32)
    message("on windows platfrom")
    unset(source_files)

    set(srcdirs
            src/json/
            src/redisClient/
            src/Win32_Interop/
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
            .
    )
    add_executable(toolAbrasionManage ${source_files})
    target_link_libraries(toolAbrasionManage hiredis paho-mqtt3a paho-mqttpp3)
else()
    message("on a unsupport platfrom")
endif ()






