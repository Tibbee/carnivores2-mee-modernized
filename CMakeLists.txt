cmake_minimum_required(VERSION 3.15)
project(Carnivores1)

# Enable modern MSVC runtime selection
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Renderer selection: SOFT or GL.
# Leave empty (unset) to build only the standalone menu executable.
set(RENDERER "" CACHE STRING "Select the renderer: SOFT or GL. Leave empty to build the menu only.")
set_property(CACHE RENDERER PROPERTY STRINGS SOFT GL "")

# Enable LTO for Release builds; CMake supplies MSVC /GL and /LTCG.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# Phase 0.1 in-engine GL perf harness. Defaults OFF. The ogl-release
# preset turns this ON because perf measurements must run on the
# optimized shipping build (debug builds are dominated by _DEBUG and
# MEM_DEBUG overhead, so the numbers are useless). To produce a
# shippable build without the harness, reconfigure with
# -DGL_PERF_HOOKS=OFF or build a different preset. Has no effect on
# SOFT builds.
option(GL_PERF_HOOKS "Enable in-engine GL performance harness (Phase 0.1)" OFF)
option(MEM_DEBUG "Enable game allocation tracking and leak reporting" OFF)

# The deployed smoke test requires licensed game assets and a copied executable.
# Keep it out of asset-free default CTest/CI runs and opt in on a developer
# machine with -DENABLE_DEPLOYED_SMOKE_TEST=ON.
option(ENABLE_DEPLOYED_SMOKE_TEST "Register the deployed game integration smoke test" OFF)
set(CARNIVORES_LEGACY_DIR "" CACHE PATH
    "Deployed C2 game directory used by the optional smoke test")

# Renderer and game-only definitions are applied to the game target below;
# keeping them target-scoped prevents the standalone menu from inheriting
# _soft/_gl, GL_PERF_HOOKS, or MEM_DEBUG.
set(RENDER_DEFINES "")
set(GAME_DEBUG_DEFINES "")

# Ensure we are targeting 32-bit if using MSVC
if(MSVC)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(WARNING "This project contains x86 inline assembly and MUST be compiled as 32-bit (x86).")
    endif()

    # Keep the legacy compiler mode while warning policy is migrated per target.
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
    add_definitions(-DWIN32)
    add_definitions(-DNOMINMAX)

    # Enable memory debugging for Debug builds by default, or explicitly for
    # any configuration through -DMEM_DEBUG=ON. Keep it game-target scoped;
    # the standalone menu intentionally remains outside MEM_DEBUG.
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR MEM_DEBUG)
        list(APPEND GAME_DEBUG_DEFINES MEM_DEBUG)
    endif()
endif()

option(C2_WARNINGS_AS_ERRORS
       "Treat project-owned MSVC warnings as errors (legacy baseline must be clean)"
       OFF)

set(C2_MSVC_COMPILE_OPTIONS)
if(MSVC)
    # /permissive remains intentionally enabled until its legacy failures are
    # catalogued and fixed. These options are target-scoped below so dependency
    # targets do not inherit the project's warning policy.
    list(APPEND C2_MSVC_COMPILE_OPTIONS /MP /permissive /W4)
    if(C2_WARNINGS_AS_ERRORS)
        list(APPEND C2_MSVC_COMPILE_OPTIONS /WX)
    endif()
endif()

function(c2_apply_msvc_compile_policy target)
    if(MSVC)
        target_compile_options(${target} PRIVATE ${C2_MSVC_COMPILE_OPTIONS})
    endif()
endfunction()

# Renderer-specific source and library configuration. Skipped entirely when
# RENDERER is empty (menu-only builds).
set(RENDER_SRC "")

if(RENDERER)
    if(RENDERER STREQUAL "SOFT")
        list(APPEND RENDER_DEFINES _soft)
        set(RENDER_SRC
            Hunt/Renderer/RenderSoft.cpp
            Hunt/Renderer/SoftInternal.h
            Hunt/Renderer/SoftHUD.cpp
            Hunt/Renderer/SoftModel.cpp
            Hunt/Renderer/SoftTerrain.cpp
            Hunt/Renderer/SoftWater.cpp
            Hunt/Renderer/SoftEntities.cpp
            Hunt/Renderer/SoftVideo.cpp
            Hunt/Renderer/SoftSky.cpp
            Hunt/Renderer/SoftMisc.cpp
            Hunt/Renderer/SoftRenderer.cpp
            Hunt/Renderer/SoftRenderer.h
        )
        message(STATUS "Building with Software Renderer")
    elseif(RENDERER STREQUAL "GL")
        list(APPEND RENDER_DEFINES _gl)
        if(GL_PERF_HOOKS)
            list(APPEND RENDER_DEFINES GL_PERF_HOOKS)
        endif()
        set(RENDER_SRC
            Hunt/Renderer/IRenderer.h
            Hunt/Renderer/GLRenderer.h
            Hunt/Renderer/GLRenderer.cpp
            Hunt/Renderer/SkyFogProjection.h
            Hunt/Renderer/GLUtils.cpp
            Hunt/Renderer/GLFog.cpp
            Hunt/Renderer/GLNight.cpp
            Hunt/Renderer/GLFlashlight.cpp
            Hunt/Renderer/GLShadow.cpp
            Hunt/Renderer/GLSky.cpp
            Hunt/Renderer/GLHUD.cpp
            Hunt/Renderer/GLModel.cpp
            Hunt/Renderer/GLTerrain.cpp
            Hunt/Renderer/GLWater.cpp
            Hunt/Renderer/GLShader.h
            Hunt/Renderer/GLShader.cpp
            Hunt/Renderer/GLPerf.h
            Hunt/Renderer/GLPerf.cpp
            Hunt/Renderer/GLUI.cpp
            deps/glad/glad.c
        )
        if(GL_PERF_HOOKS)
            message(STATUS "Building with OpenGL Renderer (GL_PERF_HOOKS=ON)")
        else()
            message(STATUS "Building with OpenGL Renderer")
        endif()
    else()
        message(FATAL_ERROR "Unknown RENDERER='${RENDERER}'. Expected SOFT, GL, or empty for menu-only builds.")
    endif()
else()
    message(STATUS "RENDERER is empty - building menu only (no game executable)")
endif()

# Set C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core source files (excluding renderers)
set(CORE_FILES
    Hunt/Audio/Audio_DLL.cpp
    Hunt/Audio/OpenAL_Loader.cpp
    Hunt/Game/Characters.cpp
    Hunt/Game/CharacterCollision.cpp
    Hunt/Game/CharacterTargeting.cpp
    Hunt/Game/CharacterMovement.cpp
    Hunt/Game/CharacterMorph.cpp
    Hunt/Game/CharacterSpawn.cpp
    Hunt/Game/CharacterAI.cpp
    Hunt/Game/CharacterAwareness.cpp
    Hunt/Game/CharacterTracking.cpp
    Hunt/Game/CharacterAnimation.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateHuntDead.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateDeadCommon.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateTitan.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimatePoacher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateHuntable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateMicro.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/huntDogSearch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateHuntdog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateTRex.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateMClientCharacter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateClassicAmbient.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateFish.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateIcth.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateDeadFish.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateIcthDead.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateBrahi.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateBrahiOld.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Game/CharacterAnimation/AnimateDimor.cpp
    Hunt/Game/Controls.cpp
    Hunt/Game/PlayerMovement.cpp
    Hunt/Game/Effects.cpp
    Hunt/Game/Network.cpp
    Hunt/Network/NetworkManager.cpp
    Hunt/Debug/Log.cpp
    Hunt/Game/TerrainQueries.cpp
    Hunt/Game/Ship.cpp
    Hunt/Game/CommandLine.cpp
    Hunt/Game/Config.cpp
    Hunt/Game/EngineInit.cpp
    Hunt/Game/Projectiles.cpp
    Hunt/Game/Ships.cpp
    Hunt/Game/Trophy.cpp
    Hunt/Game/Hunt.cpp
    Hunt/Game/Interface.cpp
    Hunt/Math/Math.cpp
    Hunt/Math/Vector.cpp
    Hunt/Loaders/Resources.cpp
    Hunt/Loaders/ScriptParser.cpp
    Hunt/Loaders/ModelLoader.cpp
    Hunt/Loaders/MapLoader.cpp
    Hunt/Loaders/SoundLoader.cpp
    Hunt/Loaders/PictureLoader.cpp
    Hunt/Loaders/CharacterLoader.cpp
    Hunt/Renderer/renderasm.cpp
    Hunt/Renderer/UIText.cpp
    Hunt/Renderer/UIText.h
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
    Hunt/Core/ConfigText.h
    Hunt/Core/Version.h
    Hunt/Core/CommandLineParse.h
    Hunt/Loaders/ScriptValueParse.h
    Hunt/Renderer/RenderContext.h
    Hunt/Renderer/RenderContext.cpp
    Hunt/AppRes.h
)


# MSVC Release already supplies /O2 (including /Oi, /Ot, /Ob2). IPO above
# supplies /GL and /LTCG; keep only the project-specific choices here.
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Gw /GS /fp:fast /arch:SSE2 /Zc:inline")
endif()

# PGO is a linker operation. Keep it on the GL game only, not the menu,
# tests, or vendored dependencies. Both presets point at the same PGD file.
set(C2_PGO_MODE "" CACHE STRING "GL game PGO mode: INSTRUMENT, OPTIMIZE, or empty")
set_property(CACHE C2_PGO_MODE PROPERTY STRINGS "" INSTRUMENT OPTIMIZE)
set(C2_PGO_PGD_FILE "" CACHE FILEPATH "Shared MSVC PGO database path")
if(C2_PGO_MODE)
    if(NOT MSVC OR NOT RENDERER STREQUAL "GL" OR NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        message(FATAL_ERROR "C2_PGO_MODE requires an MSVC GL Release build")
    endif()
    if(NOT C2_PGO_MODE STREQUAL "INSTRUMENT" AND NOT C2_PGO_MODE STREQUAL "OPTIMIZE")
        message(FATAL_ERROR "C2_PGO_MODE must be INSTRUMENT or OPTIMIZE")
    endif()
    if(NOT C2_PGO_PGD_FILE)
        message(FATAL_ERROR "C2_PGO_PGD_FILE must point to the shared .pgd file")
    endif()
endif()

# Menu subdirectory (always built - renderer-agnostic)
add_subdirectory(Menu)

# Game executable (only when a renderer is selected)
if(RENDERER)
    add_executable(Carnivores1 WIN32 ${CORE_FILES} ${RENDER_SRC})

    if(C2_PGO_MODE STREQUAL "INSTRUMENT")
        target_link_options(Carnivores1 PRIVATE "/GENPROFILE:PGD=${C2_PGO_PGD_FILE}")
    elseif(C2_PGO_MODE STREQUAL "OPTIMIZE")
        target_link_options(Carnivores1 PRIVATE "/USEPROFILE:PGD=${C2_PGO_PGD_FILE}")
    endif()

    target_compile_definitions(Carnivores1 PRIVATE
        ${RENDER_DEFINES}
        ${GAME_DEBUG_DEFINES}
    )

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
    )

    # Output directory for the executable
    set_target_properties(Carnivores1 PROPERTIES
        OUTPUT_NAME "Carnivores1_${RENDERER}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    # Copy shader files to the output directory (for the GL renderer)
    # Keep configure-time copies fresh during incremental shader iteration.
    file(GLOB C2_SHADER_INPUTS CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${C2_SHADER_INPUTS})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR}/bin)
endif()

# ============================================================
# Testing
# ============================================================
if(MSVC)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
    )
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_MakeAvailable(googletest)

    add_executable(Carnivores2Tests
        tests/test_ai_behavior_math.cpp
        tests/test_billboard_math.cpp
        tests/test_command_line_parse.cpp
        tests/test_config_parse.cpp
        tests/test_config_text.cpp
        tests/test_flashlight.cpp
        tests/test_night_lighting.cpp
        tests/test_night_presentation.cpp
        tests/test_legacy_scalar_differential.cpp
        tests/test_load_diagnostics.cpp
        tests/test_script_value_parse.cpp
        tests/test_game_mode.cpp
        tests/test_math.cpp
        Hunt/Math/Vector.cpp
        tests/test_trophy_mode.cpp
        tests/test_key_bindings.cpp
        tests/test_load_validate.cpp
        tests/test_map_scaling.cpp
        tests/test_memory_arena.cpp
        tests/test_menu_slider.cpp
        tests/test_menu_launch_args.cpp
        tests/test_shop_safety.cpp
        tests/test_projectile_math.cpp
        tests/test_score_mod.cpp
        tests/test_sky_fog_projection.cpp
        tests/test_sky_mode_mapping.cpp
        tests/test_spawn_math.cpp
        tests/test_terrain_fog.cpp
        tests/test_terrain_mip.cpp
        tests/test_unattended_capture.cpp
        tests/test_water_color.cpp
        tests/test_water_object_visibility.cpp
        tests/test_water_wave_fade.cpp
    )
    target_include_directories(Carnivores2Tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt/Core
        ${CMAKE_CURRENT_SOURCE_DIR}/Menu
    )
    target_compile_definitions(Carnivores2Tests PRIVATE
        C2_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(Carnivores2Tests PRIVATE gtest_main)
    add_test(NAME Carnivores2Tests COMMAND Carnivores2Tests)

    # Exercise the production world-model sampler without a GL context.
    # Engine globals / CalcFogLevel are controlled test doubles in this target.
    add_executable(Carnivores2FogTests
        tests/test_fog_sampling.cpp
        Hunt/Renderer/GLFog.cpp
    )
    target_include_directories(Carnivores2FogTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Hunt)
    target_link_libraries(Carnivores2FogTests PRIVATE gtest_main)
    add_test(NAME Carnivores2Tests.FogSampling COMMAND Carnivores2FogTests)

    # Exercise the production box-text layout without a renderer. Only WinH and
    # UIScale are needed as test doubles for Hunt/Renderer/UIText.cpp.
    add_executable(Carnivores2UITests
        tests/test_ui_text.cpp
        Hunt/Renderer/UIText.cpp
    )
    target_include_directories(Carnivores2UITests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Hunt)
    target_link_libraries(Carnivores2UITests PRIVATE gtest_main gdi32)
    add_test(NAME Carnivores2Tests.UIText COMMAND Carnivores2UITests)

    # Link the production WAV loader with a test halt seam so malformed/missing
    # file failures exercise the real entry point instead of only helpers.
    add_executable(Carnivores2LoaderContractTests
        tests/test_sound_loader_entry.cpp
        Hunt/Loaders/SoundLoader.cpp
    )
    target_include_directories(Carnivores2LoaderContractTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt
    )
    target_link_libraries(Carnivores2LoaderContractTests PRIVATE gtest_main)
    add_test(NAME Carnivores2Tests.LoaderContracts
             COMMAND Carnivores2LoaderContractTests)

    # Link the production BMP loader with the same narrow heap/halt seams used
    # by the game. This keeps dimension and truncation checks on the real entry
    # point rather than only testing LoadValidate helpers.
    add_executable(Carnivores2PictureLoaderContractTests
        tests/test_picture_loader_entry.cpp
        Hunt/Loaders/PictureLoader.cpp
    )
    target_include_directories(Carnivores2PictureLoaderContractTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt
    )
    target_link_libraries(Carnivores2PictureLoaderContractTests PRIVATE gtest_main)
    add_test(NAME Carnivores2Tests.PictureLoaderContracts
             COMMAND Carnivores2PictureLoaderContractTests)

    # Link the production command-line implementation with the real global
    # state definitions and a narrow log/heap seam. This verifies the bounded
    # prj=/server= behavior through ProcessCommandLine(), not only helpers.
    add_executable(Carnivores2CommandLineContractTests
        tests/test_command_line_entry.cpp
        Hunt/Game/CommandLine.cpp
        Hunt/Core/StateDefs.cpp
    )
    target_include_directories(Carnivores2CommandLineContractTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt
    )
    target_link_libraries(Carnivores2CommandLineContractTests PRIVATE gtest_main)
    add_test(NAME Carnivores2Tests.CommandLineContracts
             COMMAND Carnivores2CommandLineContractTests)

    # Link the production menu resource readers with narrow global seams.
    # The menu prefers _MENU.TXT and falls back to the legacy _RES.TXT; both
    # grammars are driven through the real ReadCharacters()/ReadWeapons()
    # entry points so a value can never select a different field.
    add_executable(Carnivores2MenuResourceTests
        tests/test_menu_resource_entry.cpp
        tests/menu_resource_seams.cpp
        Menu/Resources.cpp
    )
    target_include_directories(Carnivores2MenuResourceTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Menu
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt
    )
    target_link_libraries(Carnivores2MenuResourceTests PRIVATE gtest_main)
    add_test(NAME Carnivores2Tests.MenuResourceContracts
             COMMAND Carnivores2MenuResourceTests)

    # Link the production _RES.TXT line readers with the real global state and
    # narrow log/heap seams. ReadWeaponLine()/ReadCharacterLine() must keep
    # text values (name/file/gunshot/pic1/picc/bModel) away from the numeric
    # dispatch and the block openers so a modded path cannot halt the load.
    add_executable(Carnivores2ScriptParserContractTests
        tests/test_script_parser_entry.cpp
        Hunt/Loaders/ScriptParser.cpp
        Hunt/Core/StateDefs.cpp
    )
    target_include_directories(Carnivores2ScriptParserContractTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Hunt
    )
    target_link_libraries(Carnivores2ScriptParserContractTests PRIVATE gtest_main)
    add_test(NAME Carnivores2Tests.ScriptParserContracts
             COMMAND Carnivores2ScriptParserContractTests)

    # Asset-free fixtures for the support validator: bare model filenames and
    # the menu's _RES.TXT price fallback must not be missed.
    add_test(NAME Carnivores2Tests.HuntdatValidator
             COMMAND powershell -ExecutionPolicy Bypass -File
                 ${CMAKE_SOURCE_DIR}/tests/test_check_huntdat.ps1)
    add_test(NAME Carnivores2Tests.ShopAssetValidator
             COMMAND powershell -ExecutionPolicy Bypass -File
                 ${CMAKE_SOURCE_DIR}/tests/test_check_mod_assets.ps1)

    # This integration test intentionally targets a deployed game directory
    # containing licensed HUNTDAT assets. It is opt-in so asset-free CI runs
    # only deterministic unit tests.
    if(ENABLE_DEPLOYED_SMOKE_TEST AND TARGET Carnivores1)
        add_test(NAME SmokeTest
            COMMAND powershell -ExecutionPolicy Bypass -File
                ${CMAKE_SOURCE_DIR}/tests/smoke_test.ps1
                -Renderer ${RENDERER}
                -Duration 5
        )
        set_tests_properties(SmokeTest PROPERTIES
            LABELS "integration;requires-game-assets"
        )
        # The trophy room is launched with din=0/wep=0 (observer-style loadout);
        # run it against the deployed assets so that contract is exercised.
        add_test(NAME TrophySmokeTest
            COMMAND powershell -ExecutionPolicy Bypass -File
                ${CMAKE_SOURCE_DIR}/tests/smoke_test.ps1
                -Renderer ${RENDERER}
                -Preset trophy
                -Duration 5
        )
        set_tests_properties(TrophySmokeTest PROPERTIES
            LABELS "integration;requires-game-assets"
        )
        if(CARNIVORES_LEGACY_DIR)
            set_tests_properties(SmokeTest TrophySmokeTest PROPERTIES
                ENVIRONMENT "CARNIVORES_LEGACY_DIR=${CARNIVORES_LEGACY_DIR}"
            )
        endif()
    endif()
endif()

# Apply the project warning policy only to project-owned targets. GoogleTest
# and vendored loader targets retain their own upstream compiler settings.
foreach(C2_TARGET Carnivores1 Carnivores2Menu Carnivores2Tests Carnivores2FogTests Carnivores2UITests Carnivores2LoaderContractTests Carnivores2PictureLoaderContractTests Carnivores2CommandLineContractTests Carnivores2MenuResourceTests Carnivores2ScriptParserContractTests)
    if(TARGET ${C2_TARGET})
        c2_apply_msvc_compile_policy(${C2_TARGET})
    endif()
endforeach()
