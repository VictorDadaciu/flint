cmake_minimum_required(VERSION 3.15..4.0)

set(CMAKE_CXX_STANDARD 23)

project(flint VERSION 1.0
              DESCRIPTION "Command-line image filter tool"
              LANGUAGES C CXX)

include(FetchContent)

# Vulkan
find_package(Vulkan REQUIRED)

# STB
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb
)
FetchContent_MakeAvailable(stb)

file(GLOB_RECURSE FLINT_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/*.h)

add_executable(${PROJECT_NAME} ${FLINT_SOURCES})

target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

target_include_directories(${PROJECT_NAME} PRIVATE
    Vulkan
    ${FETCHCONTENT_BASE_DIR}/stb-src
    include/${PROJECT_NAME}
)

file(GLOB_RECURSE SHADER_MODULES ${PROJECT_SOURCE_DIR}/compute/*.comp)

add_custom_target(compile_shaders ALL
    python3 ${PROJECT_SOURCE_DIR}/compile_shaders.py ${PROJECT_SOURCE_DIR}/compute/
    DEPENDS ${SHADER_MODULES}
    COMMENT "Compiling shaders..."
    VERBATIM)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
endif()