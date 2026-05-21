cmake_minimum_required(VERSION 3.15)
project(Carnivores1)

# Enable modern MSVC runtime selection
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Renderer selection: D3D, SOFT, or 3DFX
set(RENDERER "D3D" CACHE STRING "Select the renderer: D3D, SOFT, or 3DFX")
set_property(CACHE RENDERER PROPERTY STRINGS D3D SOFT 3DFX)

# Ensure we are targeting 32-bit if using MSVC
if(MSVC)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(WARNING "This project contains x86 inline assembly and MUST be compiled as 32-bit (x86).")
    endif()

    # Relax compiler for legacy code
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
    add_definitions(-DWIN32)

    # Renderer specific defines and source management
    if(RENDERER STREQUAL "D3D")
        add_definitions(-D_d3d)
        set(RENDER_SRC
            Hunt/RendererD3D.cpp
            Hunt/RenderSoft.cpp
        )
        message(STATUS "Building with Direct3D Renderer")
    elseif(RENDERER STREQUAL "SOFT")
        add_definitions(-D_soft)
        set(RENDER_SRC
            Hunt/RenderSoft.cpp
        )
        message(STATUS "Building with Software Renderer")
    elseif(RENDERER STREQUAL "3DFX")
        add_definitions(-D_3dfx)
        set(RENDER_SRC
            Hunt/Render3DFX.cpp
            Hunt/RenderSoft.cpp
        )

        # Glide 2.x support
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps/Glide2/include)
        link_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps/Glide2/lib)
        set(GLIDE_LIBS glide2x)

        message(STATUS "Building with 3DFX Glide Renderer (using Glide2 folder)")
    endif()

    # Fix for some legacy headers and scoping
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /permissive")

    # Enable memory debugging for Debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_definitions(-DMEM_DEBUG)
    endif()
endif()

# Set C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core source files (excluding renderers)
set(CORE_FILES
    Hunt/Audio_DLL.cpp
    Hunt/Characters.cpp
    Hunt/Game.cpp
    Hunt/Hunt.cpp
    Hunt/Interface.cpp
    Hunt/Math.cpp
    Hunt/Resources.cpp
    Hunt/renderasm.cpp
    Hunt/AppRes.rc
    # Headers (optional, for IDE)
    Hunt/Hunt.h
    Hunt/AppRes.h
)

# Executable
add_executable(Carnivores1 WIN32 ${CORE_FILES} ${RENDER_SRC})

# Include directories
target_include_directories(Carnivores1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Hunt
    ${CMAKE_CURRENT_SOURCE_DIR}/deps
)

# Link libraries
target_link_libraries(Carnivores1 PRIVATE
    kernel32
    user32
    gdi32
    winmm
    ddraw
    opengl32
    ${GLIDE_LIBS}
)

# Output directory for the executable
set_target_properties(Carnivores1 PROPERTIES
    OUTPUT_NAME "Carnivores1_${RENDERER}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
