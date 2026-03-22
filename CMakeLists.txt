cmake_minimum_required(VERSION 3.20)
project(MyCross LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(
  crosshair
  WIN32
  src/main.cpp
  src/app_types.h
  src/common.h
  src/common.cpp
  src/config_store.h
  src/config_store.cpp
  src/overlay.h
  src/overlay.cpp
  src/server.h
  src/server.cpp
  src/ui_launcher.h
  src/ui_launcher.cpp
)
target_link_libraries(crosshair PRIVATE ws2_32)
target_compile_definitions(crosshair PRIVATE NOMINMAX)

if(MSVC)
  target_compile_options(crosshair PRIVATE /utf-8)
endif()

if(MINGW)
  target_link_options(crosshair PRIVATE -municode)
endif()

add_custom_target(
  copy_runtime_assets ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:crosshair>/configs"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:crosshair>/web"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/configs"
          "$<TARGET_FILE_DIR:crosshair>/configs"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/frontend"
          "$<TARGET_FILE_DIR:crosshair>/web"
  COMMENT "Copy runtime assets (configs + frontend)"
)

add_dependencies(copy_runtime_assets crosshair)
