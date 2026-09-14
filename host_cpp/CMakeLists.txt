cmake_minimum_required(VERSION 3.16)
project(rapfi_tui CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FTXUI 用 FetchContent 拉取（首次构建需要网络）。
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

include(FetchContent)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG        v5.0.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ftxui)

add_executable(rapfi_tui main.cpp)
target_link_libraries(rapfi_tui PRIVATE
  ftxui::component ftxui::dom ftxui::screen)
target_compile_options(rapfi_tui PRIVATE -Wall -Wextra)
