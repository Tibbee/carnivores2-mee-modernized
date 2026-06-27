cmake_minimum_required(VERSION 3.15)
project(Carnivores1)

# Enable modern MSVC runtime selection
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Renderer selection: D3D, SOFT, 3DFX, or GL.
# Leave empty (unset) to build only the standalone menu executable.
set(RENDERER "" CACHE STRING "Select the renderer: D3D, SOFT, 3DFX, or GL. Leave empty to build the menu only.")
set_property(CACHE RENDERER PROPERTY STRINGS D3D SOFT 3DFX GL "")

# Phase 0.1 in-engine GL perf harness. Defaults OFF. The ogl-release
# preset turns this ON because perf measurements must run on the
# optimized shipping build (debug builds are dominated by _DEBUG and
# MEM_DEBUG overhead, so the numbers are useless). To produce a
# shippable build without the harness, reconfigure with
# -DGL_PERF_HOOKS=OFF or build a different preset. Has no effect on
# D3D/SOFT/3DFX builds.
option(GL_PERF_HOOKS "Enable in-engine GL performance harness (Phase 0.1)" OFF)

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

    # Fix for some legacy headers and scoping
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /permissive")

    # Enable memory debugging for Debug builds. Wired to the main
    # game executable only; the menu target lives in Menu/CMakeLists.txt
    # which doesn't include this block, so the menu never gets
    # MEM_DEBUG (it has its own Resources.cpp with no leak detector,
    # and it doesn't allocate textures, so the report would always be
    # empty noise). Defense-in-depth: if a future change ever moves
    # the menu under this CMakeLists, wrap the add_definitions in
    # 'if(NOT TARGET Carnivores2Menu)' to keep the menu excluded.
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_definitions(-DMEM_DEBUG)
    endif()
endif()

# Renderer-specific source and library configuration. Skipped entirely when
# RENDERER is empty (menu-only builds).
set(RENDER_SRC "")
set(GLIDE_LIBS "")

if(RENDERER)
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
    elseif(RENDERER STREQUAL "GL")
        add_definitions(-D_gl)
        if(GL_PERF_HOOKS)
            add_definitions(-DGL_PERF_HOOKS)
        endif()
        set(RENDER_SRC
            Hunt/IRenderer.h
            Hunt/GLRenderer.h
            Hunt/GLRenderer.cpp
            Hunt/GLShader.h
            Hunt/GLShader.cpp
            Hunt/GLPerf.h
            Hunt/GLPerf.cpp
            Hunt/GLUI.cpp
            deps/glad/glad.c
        )
        if(GL_PERF_HOOKS)
            message(STATUS "Building with OpenGL Renderer (GL_PERF_HOOKS=ON)")
        else()
            message(STATUS "Building with OpenGL Renderer")
        endif()
    else()
        message(FATAL_ERROR "Unknown RENDERER='${RENDERER}'. Expected D3D, SOFT, 3DFX, GL, or empty for menu-only builds.")
    endif()
else()
    message(STATUS "RENDERER is empty - building menu only (no game executable)")
endif()

# Set C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core source files (excluding renderers)
set(CORE_FILES
    Hunt/Audio_DLL.cpp
    Hunt/OpenAL_Loader.cpp
    Hunt/Characters.cpp
    Hunt/Game/CharacterMorph.cpp
    Hunt/Game/CharacterSpawn.cpp
    Hunt/Game.cpp
    Hunt/Hunt.cpp
    Hunt/Interface.cpp
    Hunt/Math.cpp
    Hunt/Resources.cpp
    Hunt/renderasm.cpp
    Hunt/AppRes.rc
    Hunt/Carnivores1.manifest
    # Headers (optional, for IDE)
    Hunt/Core/StateDefs.cpp
    Hunt/Hunt.h
    Hunt/Core/Constants.h
    Hunt/Core/MathTypes.h
    Hunt/Core/AudioTypes.h
    Hunt/Core/RenderTypes.h
    Hunt/Core/ModelTypes.h
    Hunt/Core/GameTypes.h
    Hunt/Core/GameState.h
    Hunt/Core/GameMode.h
    Hunt/Renderer/RenderContext.h
    Hunt/Renderer/RenderContext.cpp
    Hunt/AppRes.h
)

# Menu subdirectory (always built - renderer-agnostic)
add_subdirectory(Menu)

# Game executable (only when a renderer is selected)
if(RENDERER)
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
endif()
