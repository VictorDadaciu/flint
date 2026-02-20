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

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/VkContext.cpp
    src/Texture.cpp
    src/Args.cpp
    include/${PROJECT_NAME}/VkContext.h
    include/${PROJECT_NAME}/Texture.h
    include/${PROJECT_NAME}/FilterUtils.h
    include/${PROJECT_NAME}/Args.h
)

target_link_libraries(${PROJECT_NAME}
    Vulkan::Vulkan
)

target_include_directories(${PROJECT_NAME} PRIVATE
    Vulkan
    ${FETCHCONTENT_BASE_DIR}/stb-src
    include/${PROJECT_NAME}
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
endif()