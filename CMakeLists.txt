cmake_minimum_required(VERSION 3.20)
project(RDM VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/windows/debug)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/windows/debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/windows/debug)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/windows/release)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/windows/release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/windows/release)


add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")


message(STATUS "src This is BINARY dir " ${PROJECT_BINARY_DIR})
message(STATUS "src This is SOURCE dir " ${PROJECT_SOURCE_DIR})
message(STATUS "debug or release: " ${CMAKE_BUILD_TYPE})

# 将dll文件复制到编译目录下
FILE(GLOB DLL_SRC "${PROJECT_SOURCE_DIR}/libs/*.dll" )
if (CMAKE_BUILD_TYPE MATCHES "Debug")
    FILE(COPY ${DLL_SRC} DESTINATION ${CMAKE_BINARY_DIR}/bin/windows/debug)
else()
    FILE(COPY ${DLL_SRC} DESTINATION ${CMAKE_BINARY_DIR}/bin/windows/release)
endif()



# 添加OpenCV库
set(OpenCV_DIR D:/Tools/OpenCV/opencv-build)
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# 添加boost库
include_directories(D:/Tools/Boost/boost_1_77_0)
link_directories(D:/Tools/Boost/boost_1_77_0/libs)



# 指定头文件搜索路径
include_directories(${CMAKE_SOURCE_DIR}/libs/lz4/lib)
include_directories(${CMAKE_SOURCE_DIR}/libs/zlib)
include_directories(${CMAKE_SOURCE_DIR}/libs/pyotherside/src)
include_directories(${CMAKE_SOURCE_DIR}/libs/qredisclient/3rdparty/asyncfuture)
include_directories(${CMAKE_SOURCE_DIR}/libs/qredisclient/3rdparty/crc16)
include_directories(${CMAKE_SOURCE_DIR}/libs/qredisclient/3rdparty/hiredis)
include_directories(${CMAKE_SOURCE_DIR}/libs/qredisclient/3rdparty/hiredis/adapters)
include_directories(${CMAKE_SOURCE_DIR}/libs/qredisclient/src/private)
include_directories(${CMAKE_SOURCE_DIR}/libs/qredisclient/src/transporters)
include_directories(${CMAKE_SOURCE_DIR}/libs/qredisclient/src/utils)
include_directories(${CMAKE_SOURCE_DIR}/libs/qredisclient/src)
# 指定引用的外部库的搜索路径
link_directories(${CMAKE_SOURCE_DIR}/libs)

MESSAGE(STATUS "----------------- Build Type: ${CMAKE_BUILD_TYPE} -------------------")
MESSAGE(INFO "--------------- Add Python -----------------")
# 如果使用的是非系统目录下的 Python 可以通过指定 Python3_ROOT_DIR 改变查找路径
if (CMAKE_BUILD_TYPE MATCHES "Debug")
    set(Python3_LIBRARIES D:/Tools/Python/libs/python39_d.lib)
else()
    set(Python3_LIBRARIES D:/Tools/Python/libs/python39.lib)
endif()
include_directories(D:/Tools/Python/include)


MESSAGE(INFO "--------------- Add thirdparty -----------------")
# ADD_SUBDIRECTORY(thirdparty)


set(QRC_SOURCE
        resources/commands.qrc
        resources/images.qrc
        resources/tr.qrc
        qml/qml.qrc
        py/py.qrc)
# qt5_add_resources(QRC_FILES ${QRC_SOURCE})  # 不能使用该语句，否则编译失败



find_package(Qt5 COMPONENTS
        Core
        Gui
        Widgets
        Network
        Sql
        Xml
        AxContainer
        PrintSupport
        Qml
        Quick
        Concurrent
        QuickWidgets
        Charts
        Svg
        REQUIRED)

# 获取当前文件夹下面符合要求的文件
FILE(GLOB SC_MODELS
        app/models/*.cpp
        app/models/*.h
        app/models/key-models/*.cpp
        app/models/key-models/*.h)

# 递归遍历文件夹下所有符合要求的文件
FILE(GLOB_RECURSE SC_MODULES
        modules/*.cpp
        modules/*.h)


set(HPP_SOURCE
        main.cpp
        app/app.cpp
        app/apputils.cpp
        app/events.cpp
        app/qmlutils.cpp
        app/qcompress.cpp
        app/app.h
        app/events.h
        app/apputils.h
        app/qmlutils.h
        app/qcompress.h
        app/darkmode.h
        ${SC_MODELS}
        ${SC_MODULES}
        thirdparty/singleheader/simdjson.h
        thirdparty/singleheader/simdjson.cpp
        ${QRC_SOURCE})

if (ANDROID)
    add_library(${PROJECT_NAME} SHARED ${HPP_SOURCE})
else()
    add_executable(${PROJECT_NAME} ${HPP_SOURCE})
endif ()


target_link_libraries(${PROJECT_NAME}
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Network
        Qt5::Sql
        Qt5::Xml
        Qt5::AxContainer
        Qt5::PrintSupport
        Qt5::Qml
        Qt5::Quick
        Qt5::Concurrent
        Qt5::QuickWidgets
        Qt5::Charts
        Qt5::Svg
        ${OpenCV_LIBS}
        ${Python3_LIBRARIES}
        pyotherside
        qredisclient
        lz4
        zlibstatic)


# 定义编译时的宏参数
target_compile_definitions(${PROJECT_NAME} PRIVATE RDM_VERSION="2021.12.01")



