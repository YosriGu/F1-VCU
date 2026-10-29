cmake_minimum_required(VERSION 3.21)
project(fs_ev_vcu VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(VCU_BUILD_TESTS "Build the host unit tests (GCC on the build machine)" OFF)
option(VCU_WERROR "Treat warnings as errors in hand-written code" ON)

set(VCU_WARNING_FLAGS
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wformat=2
    -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -Wundef -Wdouble-promotion)
if(VCU_WERROR)
  list(APPEND VCU_WARNING_FLAGS -Werror)
endif()

# ---- hand-written libraries (pure C, no hardware access) ----
add_library(can_e2e STATIC lib/can_e2e/can_e2e.c)
target_include_directories(can_e2e PUBLIC lib/can_e2e)
target_compile_options(can_e2e PRIVATE ${VCU_WARNING_FLAGS})

add_library(can_dti STATIC lib/can_dti/can_dti.c)
target_include_directories(can_dti PUBLIC lib/can_dti)
target_compile_options(can_dti PRIVATE ${VCU_WARNING_FLAGS})

# ---- generated code (can/gen, produced by can/tools/generate.py) ----
# Third-party generator output: compiled with -Wall only, never with -Werror.
set(CAN_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../can/gen)
file(GLOB CAN_GEN_SOURCES CONFIGURE_DEPENDS ${CAN_GEN_DIR}/*.c)
if(CAN_GEN_SOURCES)
  add_library(can_gen STATIC ${CAN_GEN_SOURCES})
  target_include_directories(can_gen PUBLIC ${CAN_GEN_DIR})
  target_compile_options(can_gen PRIVATE -Wall)
  target_link_libraries(can_gen PUBLIC m)   # generated encode/decode may call libm (round)
endif()

if(VCU_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
