enable_language(CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS YES)

include(CTest)

include(FetchContent)

FetchContent_Declare(
   ut2
   GIT_REPOSITORY https://github.com/boost-ext/ut2.git
   GIT_TAG main
   GIT_SHALLOW TRUE
)

add_library(ut2 INTERFACE)
target_include_directories(ut2 SYSTEM INTERFACE ${ut2_SOURCE_DIR})
add_library(ut2::ut2 ALIAS ut2)

# Done in developer mode only, so users won't be bothered by this :)
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/*.hpp")
source_group(TREE "${PROJECT_SOURCE_DIR}/include" PREFIX headers FILES ${headers})

file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")
source_group(TREE "${PROJECT_SOURCE_DIR}/src" PREFIX sources FILES ${sources})

add_executable(${PROJECT_NAME}_test ${sources} ${headers})

target_link_libraries(${PROJECT_NAME}_test PRIVATE ${PROJECT_NAME}::${PROJECT_NAME} ut2::ut2)

add_test(NAME ${PROJECT_NAME}_test COMMAND ${PROJECT_NAME}_test)
