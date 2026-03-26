cmake_minimum_required(VERSION 3.15..4.0)

set(CMAKE_CXX_STANDARD 23)

project(flint VERSION 1.0
              DESCRIPTION "Command-line image filter tool"
              LANGUAGES C CXX)

include(FetchContent)

# Vulkan
find_package(Vulkan REQUIRED)

# qlog
FetchContent_Declare(
    qlog
    GIT_REPOSITORY https://github.com/VictorDadaciu/qlog
    GIT_TAG origin/main
)

# STB
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb
)
FetchContent_MakeAvailable(stb qlog)

# shaderc
FetchContent_Declare(
    shaderc
    GIT_REPOSITORY https://github.com/google/shaderc
    GIT_TAG v2026.1
)
set(SHADERC_SKIP_TESTS ON)
set(SHADERC_SKIP_EXAMPLES ON)
FetchContent_Populate(shaderc)
execute_process(COMMAND python3 utils/git-sync-deps
                WORKING_DIRECTORY ${FETCHCONTENT_BASE_DIR}/shaderc-src)
add_subdirectory(${FETCHCONTENT_BASE_DIR}/shaderc-src)

file(GLOB_RECURSE FLINT_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/*.h)

add_executable(${PROJECT_NAME} ${FLINT_SOURCES})

target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan qlog shaderc)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${qlog_SOURCE_DIR}
    ${stb_SOURCE_DIR}
    ${shaderc_SOURCE_DIR}
    include/${PROJECT_NAME}
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
endif()