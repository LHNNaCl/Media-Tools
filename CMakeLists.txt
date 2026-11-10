cmake_minimum_required(VERSION 3.21)
project(MediaTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_PREFIX_PATH AND EXISTS "C:/Install/Qt/6.5.3/msvc2019_64")
    list(APPEND CMAKE_PREFIX_PATH "C:/Install/Qt/6.5.3/msvc2019_64")
endif()

if(MSVC)
    add_compile_options(/utf-8 /W4)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(FFMPEG_ROOT "D:/Install/FFmpeg" CACHE PATH "Directory that contains bin/ffmpeg.exe")
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/ffmpeg.exe")
    set(FFMPEG_BIN "${CMAKE_SOURCE_DIR}/third_party/ffmpeg")
else()
    set(FFMPEG_BIN "${FFMPEG_ROOT}/bin")
endif()

add_library(MediaToolDomain STATIC
    src/domain/src/logic.cpp
    src/domain/src/planners.cpp
)
target_include_directories(MediaToolDomain PUBLIC src/domain/include)

add_library(MediaToolApplication STATIC
    src/application/src/task_runner.cpp
)
target_include_directories(MediaToolApplication PUBLIC src/application/include)
target_link_libraries(MediaToolApplication PUBLIC MediaToolDomain)

add_library(MediaToolInfrastructure STATIC
    src/infrastructure/src/bundle.cpp
    src/infrastructure/src/ffmpeg_client.cpp
)
target_include_directories(MediaToolInfrastructure PUBLIC src/infrastructure/include)
target_link_libraries(MediaToolInfrastructure PUBLIC MediaToolDomain Qt6::Core)

qt_add_executable(MediaTool WIN32
    src/app/main.cpp
    src/app/engine.cpp
    src/app/engine.hpp
    src/app/main_window.cpp
    src/app/main_window.hpp
    resources/app.qrc
    resources/app.rc
)
target_include_directories(MediaTool PRIVATE src/app)
target_compile_definitions(MediaTool PRIVATE MEDIATOOL_VERSION="${PROJECT_VERSION}")
target_link_libraries(MediaTool PRIVATE MediaToolApplication MediaToolInfrastructure Qt6::Widgets)

find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
add_custom_command(TARGET MediaTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MediaTool>/ffmpeg"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MediaTool>/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/third_party/licenses" "$<TARGET_FILE_DIR:MediaTool>/licenses"
    COMMENT "Copy license files"
)
if(EXISTS "${FFMPEG_BIN}/ffmpeg.exe" AND EXISTS "${FFMPEG_BIN}/ffprobe.exe")
    add_custom_command(TARGET MediaTool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FFMPEG_BIN}/ffmpeg.exe" "$<TARGET_FILE_DIR:MediaTool>/ffmpeg/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FFMPEG_BIN}/ffprobe.exe" "$<TARGET_FILE_DIR:MediaTool>/ffmpeg/ffprobe.exe"
        COMMENT "Copy bundled FFmpeg"
    )
else()
    message(WARNING "FFmpeg was not found in ${FFMPEG_BIN}. MediaTool.exe will report that the bundled FFmpeg is missing.")
endif()
if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET MediaTool POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations "$<TARGET_FILE:MediaTool>"
        COMMENT "Deploy Qt runtime"
    )
endif()
file(GLOB MEDIATOOL_CRT_DIRS "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT")
if(MEDIATOOL_CRT_DIRS)
    list(GET MEDIATOOL_CRT_DIRS 0 MEDIATOOL_CRT_DIR)
    add_custom_command(TARGET MediaTool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${MEDIATOOL_CRT_DIR}" "$<TARGET_FILE_DIR:MediaTool>"
        COMMENT "Copy the MSVC runtime"
    )
endif()

install(TARGETS MediaTool RUNTIME DESTINATION .)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/third_party/licenses/" DESTINATION licenses)
if(EXISTS "${FFMPEG_BIN}/ffmpeg.exe")
    install(FILES "${FFMPEG_BIN}/ffmpeg.exe" "${FFMPEG_BIN}/ffprobe.exe" DESTINATION ffmpeg)
endif()

enable_testing()
add_executable(MediaToolTests
    tests/test_main.cpp
    tests/domain/domain_tests.cpp
    tests/application/application_tests.cpp
)
target_include_directories(MediaToolTests PRIVATE tests)
target_link_libraries(MediaToolTests PRIVATE MediaToolApplication)
add_test(NAME MediaToolTests COMMAND MediaToolTests)
