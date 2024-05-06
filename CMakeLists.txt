cmake_minimum_required(VERSION 3.24)

include(cmake/prelude.cmake)

project(
    zb8
    VERSION 0.2.1
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

add_library(zb8_zb8 INTERFACE)
add_library(zb8::zb8 ALIAS zb8_zb8)

if (MSVC)
   string(REGEX MATCH "\/cl(.exe)?$" matched_cl ${CMAKE_CXX_COMPILER})
   if (matched_cl)
      # for a C++ standards compliant preprocessor, not needed for clang-cl
      target_compile_options(zb8_zb8 INTERFACE "/Zc:preprocessor" /GL /permissive- /Zc:lambda)
      target_link_options(zb8_zb8 INTERFACE /LTCG /INCREMENTAL:NO)
   endif()
else()
   target_compile_options(zb8_zb8 INTERFACE "-Wno-missing-braces")
endif()

set_property(TARGET zb8_zb8 PROPERTY EXPORT_NAME ${PROJECT_NAME})

target_compile_features(zb8_zb8 INTERFACE cxx_std_20)
target_include_directories(
   zb8_zb8 ${warning_guard}
    INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

if (PROJECT_IS_TOP_LEVEL)
  include(cmake/dev-mode.cmake)
endif()