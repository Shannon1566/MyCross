cmake_minimum_required(VERSION 3.20)
project(MyCross LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

if(COMMAND qt_policy)
  qt_policy(SET QTP0001 NEW)
  qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(
  crosshair
  WIN32
  src/main.cpp
  src/app_types.h
  src/common.h
  src/common.cpp
  src/config_store.h
  src/config_store.cpp
  src/crosshair_controller.h
  src/crosshair_controller.cpp
  src/overlay.h
  src/overlay.cpp
)

qt_add_qml_module(
  crosshair
  URI MyCross
  VERSION 1.0
  QML_FILES
    qml/Main.qml
)

target_link_libraries(
  crosshair
  PRIVATE
  Qt6::Quick
  Qt6::QuickControls2
)

target_compile_definitions(crosshair PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
  target_compile_options(crosshair PRIVATE /utf-8)
endif()

get_filename_component(QT_BIN_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
if(WINDEPLOYQT_EXECUTABLE)
  add_custom_command(
    TARGET crosshair
    POST_BUILD
    COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --qmldir "${CMAKE_SOURCE_DIR}/qml"
            "$<TARGET_FILE:crosshair>"
    COMMENT "Deploy Qt runtime"
  )
endif()

add_custom_target(
  copy_runtime_assets ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:crosshair>/configs"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/configs"
          "$<TARGET_FILE_DIR:crosshair>/configs"
  COMMENT "Copy runtime assets (configs)"
)

add_dependencies(copy_runtime_assets crosshair)
