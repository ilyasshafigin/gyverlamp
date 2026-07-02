cmake_minimum_required(VERSION 3.14)
project(gyverlamp_sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SIM_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(COMMON_DIR "${SIM_DIR}/common")
set(HOST_DIR "${SIM_DIR}/host")
set(HOST_SRC_DIR "${HOST_DIR}/src")
set(HOST_SHIMS_DIR "${HOST_DIR}/shims")
set(ROOT_DIR "${SIM_DIR}/..")
set(WASM_DIR "${SIM_DIR}/wasm")

file(GLOB CATALOG_SOURCES "${ROOT_DIR}/src/effect/catalog/*.cpp")

foreach(name WIDTH HEIGHT CONNECTION_ANGLE STRIP_DIRECTION)
    if (NOT DEFINED SIM_${name} AND DEFINED ENV{SIM_${name}})
        set(SIM_${name} "$ENV{SIM_${name}}")
    endif()
endforeach()

# Shared simulator runtime consumed by the WASM target. This list is the single
# source of truth for firmware sources, host LED implementation, and host stubs.
set(SIM_COMMON_SOURCES
    ${COMMON_DIR}/sim_runtime.cpp
    ${COMMON_DIR}/sim_time.cpp
    ${HOST_SRC_DIR}/host_led.cpp
    ${HOST_SRC_DIR}/sim_audio_input.cpp
    ${HOST_SRC_DIR}/sim_stubs.cpp
    ${ROOT_DIR}/src/core/frame_renderer.cpp
    ${ROOT_DIR}/src/core/power_controller.cpp
    ${ROOT_DIR}/src/core/rotation_controller.cpp
    ${ROOT_DIR}/src/effect/catalog.cpp
    ${ROOT_DIR}/src/effect/controller.cpp
    ${ROOT_DIR}/src/effect/shared.cpp
    ${ROOT_DIR}/src/effect/math.cpp
    ${ROOT_DIR}/src/effect/palette_catalog.cpp
    ${ROOT_DIR}/src/effect/palettes.cpp
    ${ROOT_DIR}/src/effect/palette_utils.cpp
    ${ROOT_DIR}/src/storage/eeprom_store.cpp
    ${ROOT_DIR}/src/storage/settings_repository.cpp
    ${ROOT_DIR}/src/audio/audio_modulation.cpp
    ${ROOT_DIR}/src/audio/audio_service.cpp
    ${ROOT_DIR}/src/hardware/microphone.cpp
    ${ROOT_DIR}/src/util/fade_animator.cpp
    ${ROOT_DIR}/src/notification/controller.cpp
    ${ROOT_DIR}/src/notification/overlay.cpp
    ${ROOT_DIR}/src/notification/user_renderer.cpp
    ${ROOT_DIR}/src/notification/system_renderer.cpp
    ${ROOT_DIR}/src/notification/button_renderer.cpp
    ${ROOT_DIR}/src/notification/user_state.cpp
    ${ROOT_DIR}/src/text/running_text.cpp
    ${ROOT_DIR}/src/text/text_renderer.cpp
    ${CATALOG_SOURCES}
)

add_library(sim_wasm_host STATIC ${SIM_COMMON_SOURCES})

target_include_directories(sim_wasm_host PUBLIC
    ${HOST_SHIMS_DIR}
    ${COMMON_DIR}
    ${HOST_SRC_DIR}
    ${ROOT_DIR}/src
    ${ROOT_DIR}/src/effect/catalog
)

# src/effect is added as a quoted-include path only so that firmware headers
# such as "catalog.h" and "effect.h" resolve without shadowing <math.h>.
target_compile_options(sim_wasm_host PUBLIC -iquote ${ROOT_DIR}/src/effect)
target_compile_definitions(sim_wasm_host PUBLIC USE_ADC SIMULATOR_AUDIO_INPUT)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(sim_wasm_host PUBLIC -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter -O2)
endif()

foreach(name WIDTH HEIGHT CONNECTION_ANGLE STRIP_DIRECTION)
    if (DEFINED SIM_${name} AND NOT "${SIM_${name}}" STREQUAL "")
        target_compile_definitions(sim_wasm_host PUBLIC ${name}=${SIM_${name}})
    endif()
endforeach()

if (NOT EMSCRIPTEN)
    message(FATAL_ERROR "This simulator build supports Emscripten only. Use emcmake to configure it.")
endif()

# WASM side module. Pure exported ABI, no main().
add_executable(gyverlamp_sim_wasm
    ${WASM_DIR}/src/wasm_main.cpp
)

target_include_directories(gyverlamp_sim_wasm PRIVATE
    ${HOST_SHIMS_DIR}
    ${COMMON_DIR}
    ${HOST_SRC_DIR}
    ${ROOT_DIR}/src
    ${ROOT_DIR}/src/effect/catalog
)

target_compile_options(gyverlamp_sim_wasm PRIVATE -iquote ${ROOT_DIR}/src/effect)

target_link_libraries(gyverlamp_sim_wasm PRIVATE sim_wasm_host)

foreach(name WIDTH HEIGHT CONNECTION_ANGLE STRIP_DIRECTION)
    if (DEFINED SIM_${name} AND NOT "${SIM_${name}}" STREQUAL "")
        target_compile_definitions(gyverlamp_sim_wasm PRIVATE ${name}=${SIM_${name}})
    endif()
endforeach()

set_target_properties(gyverlamp_sim_wasm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ROOT_DIR}/sim/web/public/wasm"
)

target_link_options(gyverlamp_sim_wasm PRIVATE
    --no-entry
    -sMODULARIZE=1
    -sEXPORT_NAME=GyverLampSimModule
    -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue','UTF8ToString','HEAPU8']
    -sEXPORTED_FUNCTIONS=@${WASM_DIR}/exported_functions.txt
    -sALLOW_MEMORY_GROWTH=1
    -sWASM=1
    -sENVIRONMENT=web,node
    -O2
    -sSINGLE_FILE=0
)
